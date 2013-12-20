#ifndef SWC_COMPOSITOR_H
#define SWC_COMPOSITOR_H

#include "pointer.h"
#include "renderer.h"
#include "view.h"

#include <wayland-server.h>

struct swc_compositor
{
    struct wl_display * display;

    struct swc_renderer renderer;

    struct wl_list outputs;
    struct wl_list surfaces;

    /* Internal state related to repainting the screen. */
    struct
    {
        pixman_region32_t damage;
        pixman_region32_t opaque;

        /* A mask of outputs that have been repainted but are waiting on a page
         * flip. */
        uint32_t pending_flips;

        /* A mask of outputs that are scheduled to be repainted on the next
         * idle. */
        uint32_t scheduled_updates;
    };

    struct swc_view compositor_view;
    struct swc_view cursor_view;

    struct swc_pointer_handler pointer_handler;

    struct wl_listener drm_listener;
    struct wl_listener pointer_listener;
};

bool swc_compositor_initialize(struct swc_compositor * compositor,
                               struct wl_display * display,
                               struct wl_event_loop * event_loop);

void swc_compositor_finish(struct swc_compositor * compositor);

void swc_compositor_add_globals(struct swc_compositor * compositor,
                                struct wl_display * display);

void swc_compositor_schedule_update(struct swc_compositor * compositor,
                                    struct swc_output * output);

#endif

