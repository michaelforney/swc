#ifndef SWC_OUTPUT_H
#define SWC_OUTPUT_H 1

#include "buffer.h"

#include <stdint.h>
#include <pixman.h>
#include <wayland-util.h>
#include <wayland-server.h>
#include <xf86drmMode.h>

struct swc_output
{
    /* Outputs need IDs so surfaces can keep track of which output they are
     * visible on. */
    uint32_t id;

    struct swc_drm * drm;

    /* The geometry of this output */
    pixman_rectangle32_t geometry;
    uint32_t physical_width, physical_height;

    struct wl_array modes;
    struct swc_mode * current_mode, * preferred_mode;

    /* Use double buffering */
    struct swc_buffer buffers[2];
    uint8_t front_buffer;

    /* The CRTC and connector we are using to drive this output */
    uint32_t crtc_id;
    uint32_t connector_id;

    struct
    {
        drmModeCrtc * crtc;
    } original_state;

    bool repaint_scheduled;

    struct wl_list resource_list;
    struct wl_list link;
};

bool swc_output_initialize(struct swc_output * output, struct swc_drm * drm,
                           uint32_t id, uint32_t crtc_id,
                           drmModeConnector * connector);

void swc_output_finish(struct swc_output * output);

void swc_output_add_globals(struct swc_output * output,
                            struct wl_display * display);

void swc_output_switch_buffer(struct swc_output * output);

static inline struct swc_buffer * swc_output_get_front_buffer
    (struct swc_output * output)
{
    return &output->buffers[output->front_buffer];
}

static inline struct swc_buffer * swc_output_get_back_buffer
    (struct swc_output * output)
{
    return &output->buffers[!output->front_buffer];
}

#endif

