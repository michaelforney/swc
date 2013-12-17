#ifndef SWC_OUTPUT_H
#define SWC_OUTPUT_H

#include "plane.h"

#include <stdint.h>
#include <pixman.h>
#include <wayland-util.h>
#include <xf86drmMode.h>

#define SWC_OUTPUT_MASK(output) (1 << (output)->id)

struct wl_display;

struct swc_output
{
    uint32_t id;

    /* The geometry of this output */
    pixman_rectangle32_t geometry;
    uint32_t physical_width, physical_height;

    struct wl_array modes;
    struct swc_mode * current_mode, * preferred_mode;

    /* Output planes. */
    struct swc_plane framebuffer_plane;
    struct swc_plane cursor_plane;

    pixman_region32_t current_damage, previous_damage;

    /* The CRTC and connector we are using to drive this output */
    uint32_t crtc_id;
    uint32_t connector_id;

    struct
    {
        drmModeCrtc * crtc;
    } original_state;

    struct wl_list resources;
    struct wl_list link;
};

bool swc_output_initialize(struct swc_output * output, uint32_t id, uint32_t crtc_id,
                           drmModeConnector * connector);

void swc_output_finish(struct swc_output * output);

void swc_output_add_globals(struct swc_output * output,
                            struct wl_display * display);

#endif

