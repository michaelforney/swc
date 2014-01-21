#ifndef SWC_POINTER_H
#define SWC_POINTER_H

#include "buffer.h"
#include "input_focus.h"
#include "surface.h"
#include "view.h"

#include <wayland-server.h>
#include <pixman.h>

struct swc_pointer;

struct swc_pointer_handler
{
    void (* focus)(struct swc_pointer * pointer);
    bool (* motion)(struct swc_pointer * pointer, uint32_t time);
    bool (* button)(struct swc_pointer * pointer, uint32_t time,
                    uint32_t button, uint32_t state);
    bool (* axis)(struct swc_pointer * pointer, uint32_t time,
                  enum wl_pointer_axis axis, wl_fixed_t amount);
};

struct swc_pointer
{
    struct swc_input_focus focus;
    struct swc_input_focus_handler focus_handler;

    struct
    {
        struct swc_view view;
        struct wl_listener view_listener;
        struct swc_surface * surface;
        struct wl_listener destroy_listener;
        struct swc_buffer buffer;

        struct
        {
            int32_t x, y;
        } hotspot;
    } cursor;

    const struct swc_pointer_handler * handler;

    wl_fixed_t x, y;
    pixman_region32_t region;
};

bool swc_pointer_initialize(struct swc_pointer * pointer);
void swc_pointer_finish(struct swc_pointer * pointer);
void swc_pointer_set_focus(struct swc_pointer * pointer,
                           struct swc_surface * surface);
void swc_pointer_set_region(struct swc_pointer * pointer,
                            pixman_region32_t * region);
struct wl_resource * swc_pointer_bind(struct swc_pointer * pointer,
                                      struct wl_client * client, uint32_t id);
void swc_pointer_handle_button(struct swc_pointer * pointer, uint32_t time,
                               uint32_t button, uint32_t state);
void swc_pointer_handle_axis(struct swc_pointer * pointer, uint32_t time,
                             uint32_t axis, wl_fixed_t amount);
void swc_pointer_handle_relative_motion
    (struct swc_pointer * pointer, uint32_t time, wl_fixed_t dx, wl_fixed_t dy);

#endif

