#ifndef SWC_SURFACE_H
#define SWC_SURFACE_H 1

#include "surface_state.h"

#include <stdbool.h>
#include <wayland-server.h>
#include <pixman.h>

enum swc_surface_event
{
    SWC_SURFACE_ATTACH,
    SWC_SURFACE_REPAINT
};

struct swc_surface_state
{
    struct wl_buffer * buffer;

    /* The region that needs to be repainted */
    pixman_region32_t damage;

    /* ? */
    pixman_region32_t opaque;

    /* ? */
    pixman_region32_t input;

    struct wl_list frame_callbacks;
    struct wl_listener buffer_destroy_listener;
};

struct swc_surface
{
    struct wl_resource * resource;

    struct swc_surface_state state;

    union swc_renderer_surface_state renderer_state;
    struct swc_compositor_surface_state compositor_state;

    struct
    {
        struct swc_surface_state state;
        int32_t x, y;
    } pending;

    struct
    {
        uint32_t x, y;
        uint32_t width, height;
    } geometry;

    struct
    {
        uint32_t width;
        uint32_t color;
    } border;

    uint32_t output_mask;

    struct wl_signal event_signal;
    struct wl_list link;
};

struct swc_surface * swc_surface_new(struct wl_client * client, uint32_t id);

void swc_surface_send_frame_callbacks(struct swc_surface * surface,
                                      uint32_t time);

#endif

