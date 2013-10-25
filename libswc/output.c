#include "output.h"
#include "mode.h"
#include "util.h"
#include "drm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdrm/drm.h>
#include <xf86drm.h>

static void bind_output(struct wl_client * client, void * data,
                        uint32_t version, uint32_t id)
{
    struct swc_output * output = data;
    struct swc_mode * mode;
    struct wl_resource * resource;
    uint32_t flags;

    if (version >= 2)
        version = 2;

    resource = wl_resource_create(client, &wl_output_interface, version, id);
    wl_resource_set_implementation(resource, NULL, output,
                                   &swc_remove_resource);
    wl_list_insert(&output->resources, wl_resource_get_link(resource));

    wl_output_send_geometry(resource, output->geometry.x, output->geometry.y,
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

bool swc_output_initialize(struct swc_output * output, struct swc_drm * drm,
                           uint32_t id, uint32_t crtc_id,
                           drmModeConnector * connector)
{
    drmModeEncoder * encoder;
    drmModeCrtc * current_crtc;
    struct swc_mode * modes;
    uint32_t index;

    output->drm = drm;

    printf("initializing output with id: %u\n", id);

    output->id = id;
    output->physical_width = connector->mmWidth;
    output->physical_height = connector->mmHeight;

    wl_list_init(&output->resources);
    wl_array_init(&output->modes);
    pixman_region32_init(&output->previous_damage);

    output->crtc_id = crtc_id;
    output->connector_id = connector->connector_id;

    /* Determine the current CRTC of this output. */
    encoder = drmModeGetEncoder(drm->fd, connector->encoder_id);
    current_crtc = drmModeGetCrtc(drm->fd, encoder->crtc_id);
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

    output->geometry.width = output->current_mode->width;
    output->geometry.height = output->current_mode->height;

    output->original_state.crtc = current_crtc;

    /* Create output planes */
    if (!swc_plane_initialize(&output->framebuffer_plane,
                              &swc_framebuffer_plane, output))
    {
        printf("failed to initialize framebuffer plane\n");
        goto error_base;
    }

    if (!swc_plane_initialize(&output->cursor_plane, &swc_cursor_plane, output))
    {
        printf("failed to initialize cursor plane\n");
        goto error_base;
    }

    return true;

  error_base:
    return false;
}

void swc_output_finish(struct swc_output * output)
{
    struct swc_mode * mode;
    drmModeCrtc * crtc = output->original_state.crtc;

    wl_array_for_each(mode, &output->modes)
        swc_mode_finish(mode);
    wl_array_release(&output->modes);

    drmModeSetCrtc(output->drm->fd, crtc->crtc_id, crtc->buffer_id, crtc->x,
                   crtc->y, &output->connector_id, 1, &crtc->mode);
    drmModeFreeCrtc(crtc);
}

void swc_output_add_globals(struct swc_output * output,
                            struct wl_display * display)
{
    wl_global_create(display, &wl_output_interface, 2, output, &bind_output);
}

