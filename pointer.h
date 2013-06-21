#ifndef SWC_POINTER_H
#define SWC_POINTER_H 1

#include "surface.h"
#include "input.h"

#include <wayland-server.h>

struct swc_pointer;

struct swc_pointer_handler
{
    void (* focus)(struct swc_pointer * pointer);
    void (* motion)(struct swc_pointer * pointer, uint32_t time);
    void (* button)(struct swc_pointer * pointer, uint32_t time,
                    uint32_t button, uint32_t state);
};

enum swc_pointer_event
{
    SWC_POINTER_CURSOR_CHANGED
};

struct swc_pointer
{
    struct swc_input input;
    struct swc_input_handler input_handler;
    struct wl_listener focus_surface_destroy_listener;

    struct wl_signal event_signal;

    struct
    {
        struct swc_surface * surface;
        int32_t hotspot_x, hotspot_y;
    } cursor;

    struct swc_pointer_handler * handler;

    wl_fixed_t x, y;

    uint32_t button_count;
};

bool swc_pointer_initialize(struct swc_pointer * pointer);
void swc_pointer_finish(struct swc_pointer * pointer);
void swc_pointer_set_focus(struct swc_pointer * pointer,
                           struct swc_surface * surface);
struct wl_resource * swc_pointer_bind(struct swc_pointer * pointer,
                                      struct wl_client * client, uint32_t id);

#endif

