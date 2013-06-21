#include "keyboard.h"
#include "util.h"

#include <stdio.h>

bool swc_keyboard_initialize(struct swc_keyboard * keyboard)
{
    wl_list_init(&keyboard->resources);
    wl_array_init(&keyboard->keys);
    //wl_signal_init(&keyboard->focus_signal); // ?

    keyboard->focus.surface = NULL;
    keyboard->focus.resource = NULL;

    return true;
}

void swc_keyboard_finish(struct swc_keyboard * keyboard)
{
    wl_array_release(&keyboard->keys);
}

void swc_keyboard_set_focus(struct swc_keyboard * keyboard,
                            struct swc_surface * surface)
{
    struct wl_client * client;
    struct wl_display * display;
    struct wl_resource * resource;
    uint32_t serial;

    /* Unfocus previously focused surface. */
    if (keyboard->focus.resource && keyboard->focus.surface != surface)
    {
        client = wl_resource_get_client(keyboard->focus.surface->resource);
        display = wl_client_get_display(client);
        serial = wl_display_next_serial(display);
        wl_keyboard_send_leave(keyboard->focus.resource, serial,
                               keyboard->focus.surface->resource);
    }

    /* Focus new surface, if given. */
    if (surface)
    {
        client = wl_resource_get_client(surface->resource);
        resource = wl_resource_find_for_client(&keyboard->resources, client);

        printf("keyboard: focusing surface: %p\n", surface);

        if (resource)
        {
            display = wl_client_get_display(client);
            serial = wl_display_next_serial(display);
            wl_keyboard_send_enter(resource, serial, surface->resource,
                                   &keyboard->keys);

            keyboard->focus.resource = resource;
        }
        else
            keyboard->focus.resource = NULL;

        keyboard->focus.surface = surface;
    }
    else
    {
        keyboard->focus.surface = NULL;
        keyboard->focus.resource = NULL;
    }
}

struct wl_resource * swc_keyboard_bind(struct swc_keyboard * keyboard,
                                       struct wl_client * client, uint32_t id)
{
    struct wl_resource * client_resource;

    client_resource = wl_client_add_object(client, &wl_keyboard_interface,
                                           NULL, id, NULL);
    wl_resource_set_destructor(client_resource, &swc_unbind_resource);
    wl_list_insert(&keyboard->resources, &client_resource->link);

    printf("keyboard: adding client %p, resource: %p\n", client, client_resource);

    return client_resource;
}

