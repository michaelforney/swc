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
#include "event.h"
#include "region.h"
#include "util.h"
#include "drm_buffer.h"

#include <stdlib.h>
#include <stdio.h>
#include <wld/wld.h>

static pixman_box32_t infinite_extents = {
    .x1 = INT32_MIN, .y1 = INT32_MIN,
    .x2 = INT32_MAX, .y2 = INT32_MAX
};

/**
 * Removes a buffer from a surface state.
 */
static void handle_buffer_destroy(struct wl_listener * listener, void * data)
{
    struct swc_surface_state * state;

    state = swc_container_of(listener, typeof(*state), buffer_destroy_listener);
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
static bool state_set_buffer(struct swc_surface_state * state,
                             struct wl_resource * buffer)
{
    if (buffer == state->buffer)
        return false;

    if (state->buffer)
    {
        /* No longer need to worry about the old buffer being destroyed. */
        wl_list_remove(&state->buffer_destroy_listener.link);
    }

    if (buffer)
    {
        /* Need to watch the new buffer for destruction so we can remove it
         * from state. */
        wl_resource_add_destroy_listener(buffer,
                                         &state->buffer_destroy_listener);
    }

    state->buffer = buffer;

    return true;
}

static void set_size(struct swc_surface * surface,
                     uint32_t width, uint32_t height)
{
    /* Check if the surface was resized. */
    if (width != surface->geometry.width || height != surface->geometry.height)
    {
        surface->geometry.width = width;
        surface->geometry.height = height;
    }
}

static void destroy(struct wl_client * client, struct wl_resource * resource)
{
    wl_resource_destroy(resource);
}

static void attach(struct wl_client * client, struct wl_resource * resource,
                   struct wl_resource * buffer_resource, int32_t x, int32_t y)
{
    struct swc_surface * surface = wl_resource_get_user_data(resource);

    surface->pending.commit |= SWC_SURFACE_COMMIT_ATTACH;

    state_set_buffer(&surface->pending.state, buffer_resource);

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

    /* Attach */
    if (surface->pending.commit & SWC_SURFACE_COMMIT_ATTACH)
    {
        struct wl_shm_buffer * shm_buffer;
        struct swc_drm_buffer * drm_buffer;

        if (surface->state.buffer
            && surface->state.buffer != surface->pending.state.buffer)
        {
            wl_buffer_send_release(surface->state.buffer);
        }

        state_set_buffer(&surface->state, surface->pending.state.buffer);

        /* Determine size of buffer. */
        if ((shm_buffer = wl_shm_buffer_get(surface->state.buffer)))
        {
            set_size(surface, wl_shm_buffer_get_width(shm_buffer),
                     wl_shm_buffer_get_height(shm_buffer));
        }
        else if ((drm_buffer = swc_drm_buffer_get(surface->state.buffer)))
        {
            set_size(surface, drm_buffer->drawable->width,
                     drm_buffer->drawable->height);
        }
    }

    /* Damage */
    if (surface->pending.commit & SWC_SURFACE_COMMIT_DAMAGE)
    {
        pixman_region32_intersect_rect(&surface->pending.state.damage,
                                       &surface->pending.state.damage, 0, 0,
                                       surface->geometry.width,
                                       surface->geometry.height);
        pixman_region32_union(&surface->state.damage, &surface->state.damage,
                              &surface->pending.state.damage);
        pixman_region32_clear(&surface->pending.state.damage);
    }

    /* Opaque */
    if (surface->pending.commit & SWC_SURFACE_COMMIT_OPAQUE)
    {
        pixman_region32_intersect_rect(&surface->state.opaque,
                                       &surface->pending.state.opaque, 0, 0,
                                       surface->geometry.width,
                                       surface->geometry.height);
    }

    /* Input */
    if (surface->pending.commit & SWC_SURFACE_COMMIT_INPUT)
    {
        pixman_region32_intersect_rect(&surface->state.input,
                                       &surface->pending.state.input, 0, 0,
                                       surface->geometry.width,
                                       surface->geometry.height);
    }

    /* Frame */
    if (surface->pending.commit & SWC_SURFACE_COMMIT_FRAME)
    {
        wl_list_insert_list(&surface->state.frame_callbacks,
                            &surface->pending.state.frame_callbacks);
        wl_list_init(&surface->pending.state.frame_callbacks);
    }

    if (surface->class)
    {
        if (surface->pending.commit & SWC_SURFACE_COMMIT_ATTACH)
            surface->class->interface->attach(surface, surface->state.buffer);
        surface->class->interface->update(surface);
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

struct wl_surface_interface surface_implementation = {
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

    if (surface->class && surface->class->interface->remove)
        surface->class->interface->remove(surface);

    /* Finish the surface. */
    state_finish(&surface->state);
    state_finish(&surface->pending.state);

    printf("freeing surface %p\n", surface);
    free(surface);
}

/**
 * Construct a new surface, adding it to the given client as id.
 *
 * The surface will be free'd automatically when it's resource is destroyed.
 *
 * @return The newly allocated surface.
 */
struct swc_surface * swc_surface_new(struct wl_client * client, uint32_t id)
{
    struct swc_surface * surface;

    surface = malloc(sizeof *surface);

    if (!surface)
        return NULL;

    /* Initialize the surface. */
    surface->outputs = 0;
    surface->geometry.x = 0;
    surface->geometry.y = 0;
    surface->geometry.width = 0;
    surface->geometry.height = 0;
    surface->pending.commit = 0;
    surface->class = NULL;
    surface->class_state = NULL;

    state_initialize(&surface->state);
    state_initialize(&surface->pending.state);

    /* The input region should be intersected with the surface's geometry,
     * which at this point is empty. */
    pixman_region32_clear(&surface->state.input);

    wl_signal_init(&surface->event_signal);

    /* Add the surface to the client. */
    surface->resource = wl_resource_create(client, &wl_surface_interface,
                                           1, id);
    wl_resource_set_implementation(surface->resource, &surface_implementation,
                                   surface, &surface_destroy);

    return surface;
}

void swc_surface_send_frame_callbacks(struct swc_surface * surface,
                                      uint32_t time)
{
    struct wl_resource * callback;

    wl_list_for_each(callback, &surface->state.frame_callbacks, link)
    {
        wl_callback_send_done(callback, time);
        wl_resource_destroy(callback);
    }

    wl_list_init(&surface->state.frame_callbacks);
}

void swc_surface_set_class(struct swc_surface * surface,
                           const struct swc_surface_class * class)
{
    if (surface->class == class)
        return;

    if (surface->class && surface->class->interface->remove)
        surface->class->interface->remove(surface);

    surface->class = class;

    if (surface->class)
    {
        if (surface->class->interface->add
            && !surface->class->interface->add(surface))
        {
            surface->class = NULL;
            return;
        }


        surface->class->interface->attach(surface, surface->state.buffer);
        surface->class->interface->update(surface);
    }
}

void swc_surface_update(struct swc_surface * surface)
{
    if (surface->class)
        surface->class->interface->update(surface);
}

void swc_surface_move(struct swc_surface * surface, int32_t x, int32_t y)
{
    if (surface->class)
        surface->class->interface->move(surface, x, y);
}

