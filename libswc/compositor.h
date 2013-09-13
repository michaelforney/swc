#ifndef SWC_COMPOSITOR_H
#define SWC_COMPOSITOR_H 1

#include <wayland-server.h>

#include "drm.h"
#include "tty.h"
#include "seat.h"
#include "binding.h"
#include "renderer.h"

struct swc_compositor
{
    struct wl_display * display;

    struct udev * udev;

    struct swc_tty tty;
    struct swc_seat seat;
    struct swc_drm drm;
    struct swc_renderer renderer;

    struct wl_list outputs;
    struct wl_list surfaces;
    struct wl_array key_bindings;

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

    struct swc_surface_class compositor_class;
    struct swc_surface_class cursor_class;

    struct wl_listener tty_listener;
    struct wl_listener drm_listener;
    struct wl_listener pointer_listener;

    struct wl_signal destroy_signal;
};

bool swc_compositor_initialize(struct swc_compositor * compositor,
                               struct wl_display * display);

void swc_compositor_finish(struct swc_compositor * compositor);

void swc_compositor_add_globals(struct swc_compositor * compositor,
                                struct wl_display * display);

void swc_compositor_add_key_binding(struct swc_compositor * compositor,
                                    uint32_t modifiers, xkb_keysym_t key,
                                    swc_binding_handler_t handler, void * data);

void swc_compositor_schedule_update(struct swc_compositor * compositor,
                                    struct swc_output * output);

#endif

