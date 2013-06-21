#include "output.h"

#include "mode.h"
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
    struct swc_mode * mode;
    struct wl_resource * resource;
    uint32_t flags;

    resource = wl_client_add_object(client, &wl_output_interface, NULL, id,
                                    output);
    wl_list_insert(&output->resource_list, wl_resource_get_link(resource));
    wl_resource_set_destructor(resource, &swc_unbind_resource);

    wl_output_send_geometry(resource, output->x, output->y,
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
    output->repaint_scheduled = false;
    output->front_buffer = 0;

    output->physical_width = connector->mmWidth;
    output->physical_height = connector->mmHeight;

    wl_list_init(&output->resource_list);
    wl_array_init(&output->modes);

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

    output->width = output->current_mode->width;
    output->height = output->current_mode->height;

    /* Create output buffers */
    if (!swc_buffer_initialize(&output->buffers[0], drm, output->width,
                               output->height))
    {
        printf("could not initialize buffer 0 for output\n");
        goto error_base;
    }

    if (!swc_buffer_initialize(&output->buffers[1], drm, output->width,
                               output->height))
    {
        printf("could not initialize buffer 1 for output\n");
        goto error_buffer0;
    }

    output->original_state.crtc = current_crtc;

    if (drmModeSetCrtc(drm->fd, output->crtc_id, output->buffers[0].id, 0, 0,
                       &output->connector_id, 1, &output->current_mode->info) != 0)
    {
        printf("could not set crtc for output\n");
        goto error_buffer1;
    }

    return true;

  error_buffer1:
    swc_buffer_finish(&output->buffers[1], drm);
  error_buffer0:
    swc_buffer_finish(&output->buffers[0], drm);
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

    swc_buffer_finish(&output->buffers[0], output->drm);
    swc_buffer_finish(&output->buffers[1], output->drm);
}

void swc_output_add_globals(struct swc_output * output,
                            struct wl_display * display)
{
    wl_display_add_global(display, &wl_output_interface, output, &bind_output);
}

void swc_output_switch_buffer(struct swc_output * output)
{
    printf("queueing pageflip\n");

    /* Queue a page flip */
    if (drmModePageFlip(output->drm->fd, output->crtc_id,
                        swc_output_get_back_buffer(output)->id,
                        DRM_MODE_PAGE_FLIP_EVENT, output) != 0)
    {
        printf("could not queue pageflip\n");
    }
}

