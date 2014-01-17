#include "pointer.h"
#include "event.h"
#include "util.h"
#include "view.h"

#include <stdio.h>
#include <assert.h>

static void enter(struct swc_input_focus_handler * handler,
                  struct wl_resource * resource, struct swc_surface * surface)
{
    struct swc_pointer * pointer;
    struct wl_client * client;
    struct wl_display * display;
    uint32_t serial;
    wl_fixed_t surface_x, surface_y;

    pointer = CONTAINER_OF(handler, typeof(*pointer), focus_handler);
    client = wl_resource_get_client(resource);
    display = wl_client_get_display(client);
    serial = wl_display_next_serial(display);

    surface_x = pointer->x - wl_fixed_from_int(surface->view->geometry.x);
    surface_y = pointer->y - wl_fixed_from_int(surface->view->geometry.y);

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

static void handle_cursor_surface_destroy(struct wl_listener * listener,
                                          void * data)
{
    struct swc_pointer * pointer = CONTAINER_OF(listener, typeof(*pointer),
                                                cursor.destroy_listener);

    pointer->cursor.surface = NULL;
}

bool swc_pointer_initialize(struct swc_pointer * pointer)
{
    wl_signal_init(&pointer->event_signal);

    pointer->x = wl_fixed_from_int(0);
    pointer->y = wl_fixed_from_int(0);

    pointer->focus_handler.enter = &enter;
    pointer->focus_handler.leave = &leave;

    pointer->cursor.destroy_listener.notify = &handle_cursor_surface_destroy;

    swc_input_focus_initialize(&pointer->focus, &pointer->focus_handler);
    pixman_region32_init(&pointer->region);

    return true;
}

void swc_pointer_finish(struct swc_pointer * pointer)
{
    swc_input_focus_finish(&pointer->focus);
    pixman_region32_fini(&pointer->region);
}

/**
 * Sets the focus of the pointer to the specified surface.
 */
void swc_pointer_set_focus(struct swc_pointer * pointer,
                           struct swc_surface * surface)
{
    swc_input_focus_set(&pointer->focus, surface);
}

static void clip_position(struct swc_pointer * pointer,
                          wl_fixed_t fx, wl_fixed_t fy)
{
    int32_t x, y, last_x, last_y;
    pixman_box32_t box;

    x = wl_fixed_to_int(fx);
    y = wl_fixed_to_int(fy);
    last_x = wl_fixed_to_int(pointer->x);
    last_y = wl_fixed_to_int(pointer->y);

    if (!pixman_region32_contains_point(&pointer->region, x, y, NULL))
    {
        assert(pixman_region32_contains_point(&pointer->region,
                                              last_x, last_y, &box));

        /* Do some clipping. */
        x = MAX(MIN(x, box.x2 - 1), box.x1);
        y = MAX(MIN(y, box.y2 - 1), box.y1);
    }

    pointer->x = wl_fixed_from_int(x);
    pointer->y = wl_fixed_from_int(y);
}

void swc_pointer_set_region(struct swc_pointer * pointer,
                            pixman_region32_t * region)
{
    pixman_region32_copy(&pointer->region, region);
    clip_position(pointer, pointer->x, pointer->y);
}

static void set_cursor(struct wl_client * client,
                       struct wl_resource * resource, uint32_t serial,
                       struct wl_resource * surface_resource,
                       int32_t hotspot_x, int32_t hotspot_y)
{
    struct swc_pointer * pointer = wl_resource_get_user_data(resource);
    struct swc_surface * surface;
    struct swc_pointer_event_data data;

    data.old = pointer->cursor.surface;

    if (pointer->cursor.surface)
        wl_list_remove(&pointer->cursor.destroy_listener.link);

    surface = surface_resource ? wl_resource_get_user_data(surface_resource)
                               : NULL;

    if (surface)
    {
        wl_resource_add_destroy_listener(surface->resource,
                                         &pointer->cursor.destroy_listener);
    }

    pointer->cursor.surface = surface;
    pointer->cursor.hotspot_x = hotspot_x;
    pointer->cursor.hotspot_y = hotspot_y;

    data.new = pointer->cursor.surface;

    swc_send_event(&pointer->event_signal, SWC_POINTER_CURSOR_CHANGED, &data);
}

static struct wl_pointer_interface pointer_implementation = {
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

void swc_pointer_handle_button(struct swc_pointer * pointer, uint32_t time,
                               uint32_t button, uint32_t state)
{
    if ((!pointer->handler || !pointer->handler->button
         || !pointer->handler->button(pointer, time, button, state))
        && pointer->focus.resource)
    {
        struct wl_client * client
            = wl_resource_get_client(pointer->focus.resource);
        struct wl_display * display = wl_client_get_display(client);
        uint32_t serial = wl_display_next_serial(display);

        wl_pointer_send_button(pointer->focus.resource, serial, time,
                               button, state);
    }
}

void swc_pointer_handle_axis(struct swc_pointer * pointer, uint32_t time,
                             uint32_t axis, wl_fixed_t amount)
{
    if ((!pointer->handler || !pointer->handler->axis
         || !pointer->handler->axis(pointer, time, axis, amount))
        && pointer->focus.resource)
    {
        wl_pointer_send_axis(pointer->focus.resource, time, axis, amount);
    }
}

void swc_pointer_handle_relative_motion
    (struct swc_pointer * pointer, uint32_t time, wl_fixed_t dx, wl_fixed_t dy)
{
    clip_position(pointer, pointer->x + dx, pointer->y + dy);

    if (pointer->handler && pointer->handler->focus)
        pointer->handler->focus(pointer);

    if ((!pointer->handler || !pointer->handler->motion
         || !pointer->handler->motion(pointer, time))
        && pointer->focus.resource)
    {
        wl_fixed_t surface_x, surface_y;
        surface_x = pointer->x
            - wl_fixed_from_int(pointer->focus.surface->view->geometry.x);
        surface_y = pointer->y
            - wl_fixed_from_int(pointer->focus.surface->view->geometry.y);

        wl_pointer_send_motion(pointer->focus.resource, time,
                               surface_x, surface_y);
    }
}

