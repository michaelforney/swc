#include "pointer.h"
#include "util.h"
#include "event.h"

#include <stdio.h>

static void enter(struct swc_input_handler * handler,
                  struct wl_resource * resource, struct swc_surface * surface)
{
    struct swc_pointer * pointer;
    struct wl_client * client;
    struct wl_display * display;
    uint32_t serial;
    wl_fixed_t surface_x, surface_y;

    pointer = wl_container_of(handler, pointer, input_handler);
    client = wl_resource_get_client(resource);
    display = wl_client_get_display(client);
    serial = wl_display_next_serial(display);

    surface_x = pointer->x - wl_fixed_from_int(surface->geometry.x);
    surface_y = pointer->y - wl_fixed_from_int(surface->geometry.y);

    printf("-> pointer.enter: %p (%d, %d)\n", resource, surface_x, surface_y);
    wl_pointer_send_enter(resource, serial, surface->resource,
                          surface_x, surface_y);
}

static void leave(struct swc_input_handler * handler,
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

static void handle_focus_surface_destroy(struct wl_listener * listener,
                                         void * data)
{
    struct swc_pointer * pointer;

    pointer = wl_container_of(listener, pointer,
                              focus_surface_destroy_listener);

    pointer->input.focus.surface = NULL;
}

bool swc_pointer_initialize(struct swc_pointer * pointer)
{
    wl_signal_init(&pointer->event_signal);

    pointer->x = wl_fixed_from_int(0);
    pointer->y = wl_fixed_from_int(0);

    pointer->input_handler.enter = &enter;
    pointer->input_handler.leave = &leave;

    pointer->focus_surface_destroy_listener.notify
        = &handle_focus_surface_destroy;

    swc_input_initialize(&pointer->input, &pointer->input_handler);

    return true;
}

void swc_pointer_finish(struct swc_pointer * pointer)
{
    swc_input_finish(&pointer->input);
}

/**
 * Sets the focus of the pointer to the specified surface.
 */
void swc_pointer_set_focus(struct swc_pointer * pointer,
                           struct swc_surface * surface)
{
    if (surface != pointer->input.focus.surface)
    {
        if (pointer->input.focus.surface)
            wl_list_remove(&pointer->focus_surface_destroy_listener.link);

        if (surface)
            wl_resource_add_destroy_listener
                (surface->resource, &pointer->focus_surface_destroy_listener);
    }

    swc_input_set_focus(&pointer->input, surface);
}

static void set_cursor(struct wl_client * client,
                       struct wl_resource * resource, uint32_t serial,
                       struct wl_resource * surface_resource,
                       int32_t hotspot_x, int32_t hotspot_y)
{
    struct swc_pointer * pointer = wl_resource_get_user_data(resource);
    struct swc_surface * surface = wl_resource_get_user_data(surface_resource);
    struct swc_event event;

    printf("set_cursor\n");

    pointer->cursor.surface = surface;
    pointer->cursor.hotspot_x = hotspot_x;
    pointer->cursor.hotspot_y = hotspot_y;

    event.type = SWC_POINTER_CURSOR_CHANGED;
    event.data = surface;

    wl_signal_emit(&pointer->event_signal, &event);
}

struct wl_pointer_interface pointer_implementation = {
    .set_cursor = &set_cursor
};

static void unbind(struct wl_resource * resource)
{
    struct swc_pointer * pointer = wl_resource_get_user_data(resource);

    swc_input_remove_resource(&pointer->input, resource);
}

struct wl_resource * swc_pointer_bind(struct swc_pointer * pointer,
                                      struct wl_client * client, uint32_t id)
{
    struct wl_resource * client_resource;

    printf("pointer: adding client %p\n", client);

    client_resource = wl_client_add_object(client, &wl_pointer_interface,
                                           &pointer_implementation, id, pointer);
    wl_resource_set_destructor(client_resource, &unbind);
    swc_input_add_resource(&pointer->input, client_resource);

    return client_resource;
}

