#ifndef SWC_COMPOSITOR_H
#define SWC_COMPOSITOR_H

#include "pointer.h"

#include <wayland-server.h>

struct swc_screen_internal;

struct swc_compositor
{
    struct wl_display * display;
    struct wl_list views;

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

    struct swc_pointer_handler pointer_handler;
};

bool swc_compositor_initialize(struct swc_compositor * compositor,
                               struct wl_display * display,
                               struct wl_event_loop * event_loop);

void swc_compositor_finish(struct swc_compositor * compositor);

void swc_compositor_schedule_update(struct swc_compositor * compositor,
                                    struct swc_screen_internal * screen);

bool swc_compositor_add_surface(struct swc_compositor * compositor,
                                struct swc_surface * surface);
bool swc_compositor_remove_surface(struct swc_compositor * compositor,
                                   struct swc_surface * surface);

void swc_compositor_surface_show(struct swc_surface * surface);
void swc_compositor_surface_hide(struct swc_surface * surface);
void swc_compositor_surface_set_border_color(struct swc_surface * surface,
                                             uint32_t color);
void swc_compositor_surface_set_border_width(struct swc_surface * surface,
                                             uint32_t width);

#endif

