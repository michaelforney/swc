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

    uint32_t output_mask;

    struct wl_signal event_signal;
    struct wl_list link;
};

bool swc_surface_initialize(struct swc_surface * surface);

void swc_surface_finish(struct swc_surface * surface);

extern struct wl_surface_interface swc_surface_interface;

void swc_surface_destroy(struct wl_client * client,
                         struct wl_resource * resource);

void swc_surface_attach(struct wl_client * client,
                        struct wl_resource * resource,
                        struct wl_resource * buffer_resource,
                        int32_t x, int32_t y);

void swc_surface_damage(struct wl_client * client,
                        struct wl_resource * resource,
                        int32_t x, int32_t y,
                        int32_t width, int32_t height);

void swc_surface_frame(struct wl_client * client,
                       struct wl_resource * resource,
                       uint32_t callback);

void swc_surface_set_opaque_region(struct wl_client * client,
                                   struct wl_resource * resource,
                                   struct wl_resource * region_resource);

void swc_surface_set_input_region(struct wl_client * client,
                                  struct wl_resource * resource,
                                  struct wl_resource * region_resource);

void swc_surface_commit(struct wl_client * client,
                        struct wl_resource * resource);

#endif

