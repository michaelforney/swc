#include "pointer.h"
#include "util.h"
#include "event.h"

#include <stdio.h>

bool swc_pointer_initialize(struct swc_pointer * pointer)
{
    wl_list_init(&pointer->resources);
    wl_signal_init(&pointer->event_signal);

    pointer->x = wl_fixed_from_int(0);
    pointer->y = wl_fixed_from_int(0);

    pointer->focus.surface = NULL;
    pointer->focus.resource = NULL;

    return true;
}

void swc_pointer_finish(struct swc_pointer * pointer)
{
}

void swc_pointer_set_focus(struct swc_pointer * pointer,
                           struct swc_surface * surface)
{
    struct wl_client * client;
    struct wl_display * display;
    struct wl_resource * resource;
    uint32_t serial;

    /* Unfocus previously focused surface. */
    if (pointer->focus.resource && pointer->focus.surface != surface)
    {
        client = wl_resource_get_client(pointer->focus.surface->resource);
        display = wl_client_get_display(client);
        serial = wl_display_next_serial(display);
        wl_pointer_send_leave(pointer->focus.resource, serial,
                               pointer->focus.surface->resource);
    }

    /* Focus new surface, if given. */
    if (surface)
    {
        client = wl_resource_get_client(surface->resource);
        resource = wl_resource_find_for_client(&pointer->resources, client);

        printf("pointer: focusing surface: %p\n", surface);

        if (resource)
        {
            uint32_t surface_x, surface_y;

            surface_x = wl_fixed_to_int(pointer->x) - surface->geometry.x;
            surface_y = wl_fixed_to_int(pointer->y) - surface->geometry.y;

            display = wl_client_get_display(client);
            serial = wl_display_next_serial(display);
            wl_pointer_send_enter(resource, serial, surface->resource,
                                  wl_fixed_from_int(surface_x),
                                  wl_fixed_from_int(surface_y));

            pointer->focus.resource = resource;
        }

        pointer->focus.surface = surface;
    }
    else
    {
        pointer->focus.surface = NULL;
        pointer->focus.resource = NULL;
    }
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

struct wl_resource * swc_pointer_bind(struct swc_pointer * pointer,
                                      struct wl_client * client, uint32_t id)
{
    struct wl_resource * client_resource;

    printf("pointer: adding client %p\n", client);

    client_resource = wl_client_add_object(client, &wl_pointer_interface,
                                           &pointer_implementation, id, pointer);
    wl_resource_set_destructor(client_resource, &swc_unbind_resource);
    wl_list_insert(&pointer->resources,
                   wl_resource_get_user_data(client_resource));

    return client_resource;
}

