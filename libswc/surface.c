/* swc: surface.c
 *
 * Copyright (c) 2013 Michael Forney
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "surface.h"
#include "buffer.h"
#include "event.h"
#include "internal.h"
#include "output.h"
#include "region.h"
#include "screen.h"
#include "util.h"
#include "view.h"
#include "wayland_buffer.h"

#include <stdlib.h>
#include <stdio.h>
#include <wld/wld.h>

/**
 * Removes a buffer from a surface state.
 */
static void handle_buffer_destroy(struct wl_listener * listener, void * data)
{
    struct swc_surface_state * state;

    state = CONTAINER_OF(listener, typeof(*state), buffer_destroy_listener);
    state->buffer = NULL;
}

static void state_initialize(struct swc_surface_state * state)
{
    state->buffer = NULL;
    state->buffer_destroy_listener.notify = &handle_buffer_destroy;

    pixman_region32_init(&state->damage);
    pixman_region32_init(&state->opaque);
    pixman_region32_init_with_extents(&state->input, &infinite_extents);

    wl_list_init(&state->frame_callbacks);
}

static void state_finish(struct swc_surface_state * state)
{
    struct wl_resource * resource, * tmp;

    if (state->buffer)
    {
        /* Remove any buffer listeners */
        wl_list_remove(&state->buffer_destroy_listener.link);
    }

    pixman_region32_fini(&state->damage);
    pixman_region32_fini(&state->opaque);
    pixman_region32_fini(&state->input);

    /* Remove all leftover callbacks. */
    wl_list_for_each_safe(resource, tmp, &state->frame_callbacks, link)
        wl_resource_destroy(resource);
}

/**
 * In order to set the buffer of a surface state (current or pending), we need
 * to manage the destroy listeners we have for the new and old buffer.
 *
 * @return: Whether or not the buffer was changed.
 */
static void state_set_buffer(struct swc_surface_state * state,
                             struct swc_buffer * buffer)
{
    if (state->buffer)
    {
        /* No longer need to worry about the old buffer being destroyed. */
        wl_list_remove(&state->buffer_destroy_listener.link);
    }

    if (buffer)
    {
        /* Need to watch the new buffer for destruction so we can remove it
         * from state. */
        wl_signal_add(&buffer->destroy_signal, &state->buffer_destroy_listener);
    }

    state->buffer = buffer;
}

static void destroy(struct wl_client * client, struct wl_resource * resource)
{
    wl_resource_destroy(resource);
}

static void attach(struct wl_client * client, struct wl_resource * resource,
                   struct wl_resource * buffer_resource, int32_t x, int32_t y)
{
    struct swc_surface * surface = wl_resource_get_user_data(resource);
    struct swc_buffer * buffer
        = buffer_resource ? swc_wayland_buffer_get(buffer_resource) : NULL;

    surface->pending.commit |= SWC_SURFACE_COMMIT_ATTACH;

    state_set_buffer(&surface->pending.state, buffer);

    surface->pending.x = x;
    surface->pending.y = y;
}

static void damage(struct wl_client * client, struct wl_resource * resource,
                   int32_t x, int32_t y, int32_t width, int32_t height)
{
    struct swc_surface * surface = wl_resource_get_user_data(resource);

    surface->pending.commit |= SWC_SURFACE_COMMIT_DAMAGE;

    pixman_region32_union_rect(&surface->pending.state.damage,
                               &surface->pending.state.damage,
                               x, y, width, height);
}

static void frame(struct wl_client * client, struct wl_resource * resource,
                  uint32_t id)
{
    struct swc_surface * surface = wl_resource_get_user_data(resource);
    struct wl_resource * callback_resource;

    surface->pending.commit |= SWC_SURFACE_COMMIT_FRAME;

    callback_resource = wl_resource_create(client, &wl_callback_interface,
                                           1, id);
    wl_resource_set_implementation(callback_resource, NULL, NULL,
                                   &swc_remove_resource);
    wl_list_insert(surface->pending.state.frame_callbacks.prev,
                   wl_resource_get_link(callback_resource));
}

static void set_opaque_region(struct wl_client * client,
                              struct wl_resource * resource,
                              struct wl_resource * region_resource)
{
    struct swc_surface * surface = wl_resource_get_user_data(resource);

    surface->pending.commit |= SWC_SURFACE_COMMIT_OPAQUE;

    if (region_resource)
    {
        struct swc_region * region = wl_resource_get_user_data(region_resource);

        pixman_region32_copy(&surface->pending.state.opaque, &region->region);
    }
    else
        pixman_region32_clear(&surface->pending.state.opaque);
}

static void set_input_region(struct wl_client * client,
                             struct wl_resource * resource,
                             struct wl_resource * region_resource)
{
    struct swc_surface * surface = wl_resource_get_user_data(resource);

    surface->pending.commit |= SWC_SURFACE_COMMIT_INPUT;

    if (region_resource)
    {
        struct swc_region * region = wl_resource_get_user_data(region_resource);

        pixman_region32_copy(&surface->pending.state.input, &region->region);
    }
    else
        pixman_region32_reset(&surface->pending.state.input, &infinite_extents);
}

static void commit(struct wl_client * client, struct wl_resource * resource)
{
    struct swc_surface * surface = wl_resource_get_user_data(resource);
    struct wld_buffer * buffer;

    /* Attach */
    if (surface->pending.commit & SWC_SURFACE_COMMIT_ATTACH)
    {
        struct swc_buffer * current_buffer = surface->state.buffer,
                          * pending_buffer = surface->pending.state.buffer;

        if (current_buffer && current_buffer != pending_buffer)
            swc_wayland_buffer_release(current_buffer);

        state_set_buffer(&surface->state, surface->pending.state.buffer);
    }

    buffer = surface->state.buffer ? surface->state.buffer->wld : NULL;

    /* Damage */
    if (surface->pending.commit & SWC_SURFACE_COMMIT_DAMAGE)
    {
        pixman_region32_intersect_rect(&surface->pending.state.damage,
                                       &surface->pending.state.damage, 0, 0,
                                       buffer ? buffer->width : 0,
                                       buffer ? buffer->height : 0);
        pixman_region32_union(&surface->state.damage, &surface->state.damage,
                              &surface->pending.state.damage);
        pixman_region32_clear(&surface->pending.state.damage);
    }

    /* Opaque */
    if (surface->pending.commit & SWC_SURFACE_COMMIT_OPAQUE)
    {
        pixman_region32_intersect_rect(&surface->state.opaque,
                                       &surface->pending.state.opaque, 0, 0,
                                       buffer ? buffer->width : 0,
                                       buffer ? buffer->height : 0);
    }

    /* Input */
    if (surface->pending.commit & SWC_SURFACE_COMMIT_INPUT)
    {
        pixman_region32_copy(&surface->state.input,
                             &surface->pending.state.input);
    }

    /* Frame */
    if (surface->pending.commit & SWC_SURFACE_COMMIT_FRAME)
    {
        wl_list_insert_list(&surface->state.frame_callbacks,
                            &surface->pending.state.frame_callbacks);
        wl_list_init(&surface->pending.state.frame_callbacks);
    }

    if (surface->view)
    {
        if (surface->pending.commit & SWC_SURFACE_COMMIT_ATTACH)
            swc_view_attach(surface->view, surface->state.buffer);
        swc_view_update(surface->view);
    }

    surface->pending.commit = 0;
}

void set_buffer_transform(struct wl_client * client,
                          struct wl_resource * surface, int32_t transform)
{
    /* TODO: Implement */
}

void set_buffer_scale(struct wl_client * client, struct wl_resource * surface,
                      int32_t scale)
{
    /* TODO: Implement */
}

static struct wl_surface_interface surface_implementation = {
    .destroy = &destroy,
    .attach = &attach,
    .damage = &damage,
    .frame = &frame,
    .set_opaque_region = &set_opaque_region,
    .set_input_region = &set_input_region,
    .commit = &commit,
    .set_buffer_transform = &set_buffer_transform,
    .set_buffer_scale = &set_buffer_scale
};

static void surface_destroy(struct wl_resource * resource)
{
    struct swc_surface * surface = wl_resource_get_user_data(resource);

    /* Finish the surface. */
    state_finish(&surface->state);
    state_finish(&surface->pending.state);

    if (surface->view)
        wl_list_remove(&surface->view_listener.link);

    printf("freeing surface %p\n", surface);
    free(surface);
}

static void handle_view_event(struct wl_listener * listener, void * data)
{
    struct swc_surface * surface
        = CONTAINER_OF(listener, typeof(*surface), view_listener);
    struct swc_event * event = data;
    struct swc_view_event_data * event_data = event->data;

    switch (event->type)
    {
        case SWC_VIEW_EVENT_FRAME:
        {
            struct wl_resource * resource, * tmp;

            wl_list_for_each_safe(resource, tmp,
                                  &surface->state.frame_callbacks, link)
            {
                wl_callback_send_done(resource, event_data->frame.time);
                wl_resource_destroy(resource);
            }

            wl_list_init(&surface->state.frame_callbacks);
            break;
        }
        case SWC_VIEW_EVENT_SCREENS_CHANGED:
        {
            struct swc_screen_internal * screen;
            struct swc_output * output;
            struct wl_client * client;
            struct wl_resource * resource;
            uint32_t entered = event_data->screens_changed.entered,
                     left = event_data->screens_changed.left;

            client = wl_resource_get_client(surface->resource);

            wl_list_for_each(screen, &swc.screens, link)
            {
                if (!((entered | left) & swc_screen_mask(screen)))
                    continue;

                wl_list_for_each(output, &screen->outputs, link)
                {
                    resource = wl_resource_find_for_client
                        (&output->resources, client);

                    if (resource)
                    {
                        if (entered & swc_screen_mask(screen))
                            wl_surface_send_enter(surface->resource, resource);
                        else if (left & swc_screen_mask(screen))
                            wl_surface_send_leave(surface->resource, resource);
                    }
                }
            }
            break;
        }
        case SWC_VIEW_EVENT_RESIZED:
            pixman_region32_intersect_rect
                (&surface->state.opaque, &surface->state.opaque, 0, 0,
                 surface->view->geometry.width, surface->view->geometry.height);
            pixman_region32_intersect_rect
                (&surface->state.damage, &surface->state.damage, 0, 0,
                 surface->view->geometry.width, surface->view->geometry.height);
            break;
    }
}

/**
 * Construct a new surface, adding it to the given client as id.
 *
 * The surface will be free'd automatically when it's resource is destroyed.
 *
 * @return The newly allocated surface.
 */
struct swc_surface * swc_surface_new(struct wl_client * client,
                                     uint32_t version, uint32_t id)
{
    struct swc_surface * surface;

    surface = malloc(sizeof *surface);

    if (!surface)
        return NULL;

    /* Initialize the surface. */
    surface->pending.commit = 0;
    surface->window = NULL;
    surface->view = NULL;
    surface->view_listener.notify = &handle_view_event;

    state_initialize(&surface->state);
    state_initialize(&surface->pending.state);

    /* Add the surface to the client. */
    surface->resource = wl_resource_create(client, &wl_surface_interface,
                                           version, id);
    wl_resource_set_implementation(surface->resource, &surface_implementation,
                                   surface, &surface_destroy);

    return surface;
}

void swc_surface_set_view(struct swc_surface * surface, struct swc_view * view)
{
    if (surface->view == view)
        return;

    if (surface->view)
        wl_list_remove(&surface->view_listener.link);

    surface->view = view;

    if (view)
    {
        wl_signal_add(&view->event_signal, &surface->view_listener);
        swc_view_attach(view, surface->state.buffer);
        swc_view_update(surface->view);
    }
}

