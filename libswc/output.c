#include "output.h"
#include "drm.h"
#include "internal.h"
#include "mode.h"
#include "screen.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdrm/drm.h>
#include <xf86drm.h>

static void bind_output(struct wl_client * client, void * data,
                        uint32_t version, uint32_t id)
{
    struct swc_output * output = data;
    struct swc_screen_internal * screen = output->screen;
    struct swc_mode * mode;
    struct wl_resource * resource;
    uint32_t flags;

    if (version >= 2)
        version = 2;

    resource = wl_resource_create(client, &wl_output_interface, version, id);
    wl_resource_set_implementation(resource, NULL, output,
                                   &swc_remove_resource);
    wl_list_insert(&output->resources, wl_resource_get_link(resource));

    wl_output_send_geometry
        (resource, screen->base.geometry.x, screen->base.geometry.y,
         output->physical_width, output->physical_height, 0, "unknown",
         "unknown", WL_OUTPUT_TRANSFORM_NORMAL);

    wl_array_for_each(mode, &output->modes)
    {
        flags = 0;
        if (mode->preferred)
            flags |= WL_OUTPUT_MODE_PREFERRED;
        if (output->current_mode == mode)
            flags |= WL_OUTPUT_MODE_CURRENT;

        wl_output_send_mode(resource, flags, mode->width, mode->height,
                            mode->refresh);
    }

    if (version >= 2)
        wl_output_send_done(resource);
}

struct swc_output * swc_output_new(uint32_t crtc_id, drmModeConnector * connector)
{
    struct swc_output * output;
    drmModeEncoder * encoder;
    drmModeCrtc * current_crtc;
    struct swc_mode * modes;
    uint32_t index;

    if (!(output = malloc(sizeof *output)))
    {
        ERROR("Failed to allocated output\n");
        goto error0;
    }

    output->global = wl_global_create(swc.display, &wl_output_interface, 2,
                                      output, &bind_output);

    if (!output->global)
    {
        ERROR("Failed to create output global\n");
        goto error1;
    }

    output->physical_width = connector->mmWidth;
    output->physical_height = connector->mmHeight;

    wl_list_init(&output->resources);
    wl_array_init(&output->modes);
    pixman_region32_init(&output->current_damage);
    pixman_region32_init(&output->previous_damage);

    output->crtc = crtc_id;
    output->connector = connector->connector_id;

    /* Determine the current CRTC of this output. */
    encoder = drmModeGetEncoder(swc.drm->fd, connector->encoder_id);
    current_crtc = drmModeGetCrtc(swc.drm->fd, encoder->crtc_id);
    drmModeFreeEncoder(encoder);

    modes = wl_array_add(&output->modes, connector->count_modes * sizeof *modes);

    for (index = 0; index < connector->count_modes; ++index)
    {
        swc_mode_initialize(&modes[index], &connector->modes[index]);

        if (memcmp(&modes[index].info, &current_crtc->mode,
                   sizeof(drmModeModeInfo)) == 0)
            output->current_mode = &modes[index];
        if (modes[index].preferred)
            output->preferred_mode = &modes[index];
    }

    if (output->preferred_mode)
        output->current_mode = output->preferred_mode;

    output->original_state.crtc = current_crtc;

    /* Create output planes */
    if (!swc_plane_initialize(&output->framebuffer_plane,
                              &swc_framebuffer_plane, output))
    {
        printf("failed to initialize framebuffer plane\n");
        goto error2;
    }

    if (!swc_plane_initialize(&output->cursor_plane, &swc_cursor_plane, output))
    {
        printf("failed to initialize cursor plane\n");
        goto error2;
    }

    return output;

  error2:
    wl_global_destroy(output->global);
  error1:
    free(output);
  error0:
    return NULL;
}

void swc_output_destroy(struct swc_output * output)
{
    struct swc_mode * mode;
    drmModeCrtc * crtc = output->original_state.crtc;

    wl_array_for_each(mode, &output->modes)
        swc_mode_finish(mode);
    wl_array_release(&output->modes);

    drmModeSetCrtc(swc.drm->fd, crtc->crtc_id, crtc->buffer_id, crtc->x,
                   crtc->y, &output->connector, 1, &crtc->mode);
    drmModeFreeCrtc(crtc);
    wl_global_destroy(output->global);
    free(output);
}

