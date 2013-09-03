#include "pointer.h"
#include "util.h"
#include "event.h"

#include <stdio.h>

static void enter(struct swc_input_focus_handler * handler,
                  struct wl_resource * resource, struct swc_surface * surface)
{
    struct swc_pointer * pointer;
    struct wl_client * client;
    struct wl_display * display;
    uint32_t serial;
    wl_fixed_t surface_x, surface_y;

    pointer = swc_container_of(handler, typeof(*pointer), focus_handler);
    client = wl_resource_get_client(resource);
    display = wl_client_get_display(client);
    serial = wl_display_next_serial(display);

    surface_x = pointer->x - wl_fixed_from_int(surface->geometry.x);
    surface_y = pointer->y - wl_fixed_from_int(surface->geometry.y);

    printf("-> pointer.enter: %p (%d, %d)\n", resource, surface_x, surface_y);
    wl_pointer_send_enter(resource, serial, surface->resource,
                          surface_x, surface_y);
}

static void leave(struct swc_input_focus_handler * handler,
                  struct wl_resource * resource, struct swc_surface * surface)
{
    struct wl_client * client;
    struct wl_display * display;
    uint32_t serial;

    client = wl_resource_get_client(resource);
    display = wl_client_get_display(client);
    serial = wl_display_next_serial(display);

    printf("-> pointer.leave: %p\n", resource);
    wl_pointer_send_leave(resource, serial, surface->resource);
}

bool swc_pointer_initialize(struct swc_pointer * pointer)
{
    wl_signal_init(&pointer->event_signal);

    pointer->x = wl_fixed_from_int(0);
    pointer->y = wl_fixed_from_int(0);

    pointer->focus_handler.enter = &enter;
    pointer->focus_handler.leave = &leave;

    swc_input_focus_initialize(&pointer->focus, &pointer->focus_handler);

    return true;
}

void swc_pointer_finish(struct swc_pointer * pointer)
{
    swc_input_focus_finish(&pointer->focus);
}

/**
 * Sets the focus of the pointer to the specified surface.
 */
void swc_pointer_set_focus(struct swc_pointer * pointer,
                           struct swc_surface * surface)
{
    swc_input_focus_set(&pointer->focus, surface);
}

static void set_cursor(struct wl_client * client,
                       struct wl_resource * resource, uint32_t serial,
                       struct wl_resource * surface_resource,
                       int32_t hotspot_x, int32_t hotspot_y)
{
    struct swc_pointer * pointer = wl_resource_get_user_data(resource);
    struct swc_surface * surface;

    printf("set_cursor\n");

    surface = surface_resource ? wl_resource_get_user_data(surface_resource)
                               : NULL;

    if (surface)
    {
        surface->geometry.x = wl_fixed_to_int(pointer->x) - hotspot_x;
        surface->geometry.y = wl_fixed_to_int(pointer->y) - hotspot_y;
    }

    pointer->cursor.surface = surface;
    pointer->cursor.hotspot_x = hotspot_x;
    pointer->cursor.hotspot_y = hotspot_y;

    swc_send_event(&pointer->event_signal, SWC_POINTER_CURSOR_CHANGED, pointer);
}

struct wl_pointer_interface pointer_implementation = {
    .set_cursor = &set_cursor
};

static void unbind(struct wl_resource * resource)
{
    struct swc_pointer * pointer = wl_resource_get_user_data(resource);

    swc_input_focus_remove_resource(&pointer->focus, resource);
}

struct wl_resource * swc_pointer_bind(struct swc_pointer * pointer,
                                      struct wl_client * client, uint32_t id)
{
    struct wl_resource * client_resource;

    printf("pointer: adding client %p\n", client);

    client_resource = wl_resource_create(client, &wl_pointer_interface, 1, id);
    wl_resource_set_implementation(client_resource, &pointer_implementation,
                                   pointer, &unbind);
    swc_input_focus_add_resource(&pointer->focus, client_resource);

    return client_resource;
}

