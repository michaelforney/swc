#include "pointer.h"
#include "event.h"
#include "internal.h"
#include "screen.h"
#include "shm.h"
#include "util.h"

#include <stdio.h>
#include <assert.h>
#include <wld/wld.h>

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

    swc_view_attach(&pointer->cursor.view, NULL);
}

static bool update(struct swc_view * view)
{
    return true;
}

static bool attach(struct swc_view * view, struct swc_buffer * buffer)
{
    struct swc_pointer * pointer
        = CONTAINER_OF(view, typeof(*pointer), cursor.view);
    struct swc_surface * surface = pointer->cursor.surface;

    if (surface && !pixman_region32_not_empty(&surface->state.damage))
        return true;

    wld_set_target_buffer(swc.shm->renderer, pointer->cursor.buffer.wld);
    wld_fill_rectangle(swc.shm->renderer, 0x00000000, 0, 0, 64, 64);
    wld_copy_rectangle(swc.shm->renderer, buffer->wld, 0, 0, 0, 0,
                       buffer->wld->width, buffer->wld->height);
    wld_flush(swc.shm->renderer);

    if (surface)
        pixman_region32_clear(&surface->state.damage);

    /* TODO: Send an early release to the buffer */

    return true;
}

static bool move(struct swc_view * view, int32_t x, int32_t y)
{
    return true;
}

static const struct swc_view_impl view_impl = {
    .update = &update,
    .attach = &attach,
    .move = &move,
};

static void handle_view_event(struct wl_listener * listener, void * data)
{
    struct swc_pointer * pointer
        = CONTAINER_OF(listener, typeof(*pointer), cursor.view_listener);
    struct swc_event * event = data;
    struct swc_view_event_data * event_data = event->data;
    struct swc_view * view = event_data->view;

    switch (event->type)
    {
        case SWC_VIEW_EVENT_MOVED:
        {
            struct swc_screen_internal * screen;

            wl_list_for_each(screen, &swc.screens, link)
            {
                if (view->screens & swc_screen_mask(screen))
                {
                    struct swc_pointer * pointer
                        = CONTAINER_OF(view, typeof(*pointer), cursor.view);

                    swc_view_move(&screen->planes.cursor.view,
                                  view->geometry.x - screen->base.geometry.x,
                                  view->geometry.y - screen->base.geometry.y);

                    if (!screen->planes.cursor.view.buffer)
                    {
                        swc_view_attach(&screen->planes.cursor.view,
                                        &pointer->cursor.buffer);
                    }
                }
                else if (screen->planes.cursor.view.buffer)
                    swc_view_attach(&screen->planes.cursor.view, NULL);
            }
            break;
        }
        case SWC_VIEW_EVENT_SCREENS_CHANGED:
        {
            struct swc_screen_internal * screen;
            uint32_t entered = event_data->screens_changed.entered,
                     left = event_data->screens_changed.left;

            wl_list_for_each(screen, &swc.screens, link)
            {
                if (entered & swc_screen_mask(screen))
                {
                    swc_view_attach(&screen->planes.cursor.view,
                                    &pointer->cursor.buffer);
                }
                else if (left & swc_screen_mask(screen))
                    swc_view_attach(&screen->planes.cursor.view, NULL);
            }
            break;
        }
    }
}

bool swc_pointer_initialize(struct swc_pointer * pointer)
{
    struct wld_buffer * buffer;

    pointer->x = wl_fixed_from_int(0);
    pointer->y = wl_fixed_from_int(0);

    pointer->focus_handler.enter = &enter;
    pointer->focus_handler.leave = &leave;

    swc_view_initialize(&pointer->cursor.view, &view_impl);
    pointer->cursor.view_listener.notify = &handle_view_event;
    wl_signal_add(&pointer->cursor.view.event_signal,
                  &pointer->cursor.view_listener);
    pointer->cursor.surface = NULL;
    pointer->cursor.destroy_listener.notify = &handle_cursor_surface_destroy;
    buffer = wld_create_buffer(swc.drm->context, 64, 64, WLD_FORMAT_ARGB8888);

    if (!buffer)
        return false;

    swc_buffer_initialize(&pointer->cursor.buffer, buffer);

    swc_input_focus_initialize(&pointer->focus, &pointer->focus_handler);
    pixman_region32_init(&pointer->region);

    return true;
}

void swc_pointer_finalize(struct swc_pointer * pointer)
{
    swc_input_focus_finalize(&pointer->focus);
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

    if (pointer->cursor.surface)
        wl_list_remove(&pointer->cursor.destroy_listener.link);

    surface = surface_resource ? wl_resource_get_user_data(surface_resource)
                               : NULL;
    pointer->cursor.surface = surface;
    pointer->cursor.hotspot.x = hotspot_x;
    pointer->cursor.hotspot.y = hotspot_y;

    if (surface)
    {
        swc_surface_set_view(surface, &pointer->cursor.view);
        wl_resource_add_destroy_listener(surface->resource,
                                         &pointer->cursor.destroy_listener);
        swc_view_move(&pointer->cursor.view,
                      wl_fixed_to_int(pointer->x) - hotspot_x,
                      wl_fixed_to_int(pointer->y) - hotspot_y);
    }
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

    swc_view_move(&pointer->cursor.view,
                  wl_fixed_to_int(pointer->x) - pointer->cursor.hotspot.x,
                  wl_fixed_to_int(pointer->y) - pointer->cursor.hotspot.y);
}

