#include "keyboard.h"
#include "util.h"

#include <stdio.h>

static void enter(struct swc_input_handler * handler,
                  struct wl_resource * resource, struct swc_surface * surface)
{
    struct swc_keyboard * keyboard;
    struct wl_client * client;
    struct wl_display * display;
    uint32_t serial;

    keyboard = wl_container_of(handler, keyboard, input_handler);
    client = wl_resource_get_client(resource);
    display = wl_client_get_display(client);
    serial = wl_display_next_serial(display);

    wl_keyboard_send_enter(resource, serial, surface->resource,
                           &keyboard->keys);
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

    wl_keyboard_send_leave(resource, serial, surface->resource);
}

bool swc_keyboard_initialize(struct swc_keyboard * keyboard)
{
    wl_array_init(&keyboard->keys);

    keyboard->input_handler.enter = &enter;
    keyboard->input_handler.leave = &leave;

    swc_input_initialize(&keyboard->input, &keyboard->input_handler);

    return true;
}

void swc_keyboard_finish(struct swc_keyboard * keyboard)
{
    wl_array_release(&keyboard->keys);
    swc_input_finish(&keyboard->input);
}

/**
 * Sets the focus of the keyboard to the specified surface.
 */
void swc_keyboard_set_focus(struct swc_keyboard * keyboard,
                            struct swc_surface * surface)
{
    swc_input_set_focus(&keyboard->input, surface);
}

static void unbind(struct wl_resource * resource)
{
    struct swc_keyboard * keyboard = wl_resource_get_user_data(resource);

    swc_input_remove_resource(&keyboard->input, resource);
}

struct wl_resource * swc_keyboard_bind(struct swc_keyboard * keyboard,
                                       struct wl_client * client, uint32_t id)
{
    struct wl_resource * client_resource;

    client_resource = wl_client_add_object(client, &wl_keyboard_interface,
                                           NULL, id, keyboard);
    wl_resource_set_destructor(client_resource, &unbind);
    swc_input_add_resource(&keyboard->input, client_resource);

    return client_resource;
}

