#include "surface.h"
#include "event.h"
#include "region.h"

#include <stdlib.h>
#include <stdio.h>

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

    state = wl_container_of(listener, state, buffer_destroy_listener);
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
                             struct wl_buffer * buffer)
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
        wl_resource_add_destroy_listener(&buffer->resource,
                                         &state->buffer_destroy_listener);
    }

    state->buffer = buffer;

    return true;
}

static void destroy(struct wl_client * client, struct wl_resource * resource)
{
    wl_resource_destroy(resource);
}

static void attach(struct wl_client * client, struct wl_resource * resource,
                   struct wl_resource * buffer_resource, int32_t x, int32_t y)
{
    struct swc_surface * surface = wl_resource_get_user_data(resource);
    struct wl_buffer * buffer
        = buffer_resource ? wl_resource_get_user_data(buffer_resource) : NULL;

    state_set_buffer(&surface->pending.state, buffer);

    /* Adjust geometry of the surface to match the buffer. */
    surface->geometry.width = buffer ? buffer->width : 0;
    surface->geometry.height = buffer ? buffer->height : 0;

    surface->pending.x = x;
    surface->pending.y = y;
}

static void damage(struct wl_client * client, struct wl_resource * resource,
                   int32_t x, int32_t y, int32_t width, int32_t height)
{
    struct swc_surface * surface = wl_resource_get_user_data(resource);

    pixman_region32_union_rect(&surface->pending.state.damage,
                               &surface->pending.state.damage,
                               x, y, width, height);
}

static void frame(struct wl_client * client, struct wl_resource * resource,
                  uint32_t id)
{
    struct swc_surface * surface = wl_resource_get_user_data(resource);
    struct wl_resource * callback_resource;

    callback_resource = wl_client_add_object(client, &wl_callback_interface,
                                             NULL, id, NULL);
    wl_list_insert(surface->pending.state.frame_callbacks.prev,
                   wl_resource_get_link(callback_resource));
}

static void set_opaque_region(struct wl_client * client,
                              struct wl_resource * resource,
                              struct wl_resource * region_resource)
{
    struct swc_surface * surface = wl_resource_get_user_data(resource);

    //printf("surface_set_opaque_region\n");

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

    printf("surface.set_input_region\n");

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
    struct swc_event event;

    event.data = surface;

    /* Attach */
    if (surface->state.buffer != surface->pending.state.buffer)
    {
        if (surface->state.buffer)
        {
            /* Release the old buffer, it's no longer needed. */
            wl_buffer_send_release(&surface->state.buffer->resource);
        }

        state_set_buffer(&surface->state, surface->pending.state.buffer);

        event.type = SWC_SURFACE_ATTACH;
        wl_signal_emit(&surface->event_signal, &event);
    }

    pixman_region32_union(&surface->state.damage, &surface->state.damage,
                          &surface->pending.state.damage);
    pixman_region32_intersect_rect(&surface->state.damage,
                                   &surface->state.damage, 0, 0,
                                   surface->geometry.width,
                                   surface->geometry.height);

    /* Input */
    pixman_region32_copy(&surface->state.input, &surface->pending.state.input);
    pixman_region32_intersect_rect(&surface->state.input,
                                   &surface->state.input, 0, 0,
                                   surface->geometry.width,
                                   surface->geometry.height);

    wl_list_insert_list(&surface->state.frame_callbacks,
                        &surface->pending.state.frame_callbacks);

    /* Reset pending state */
    pixman_region32_clear(&surface->pending.state.damage);
    pixman_region32_clear(&surface->pending.state.opaque);
    surface->pending.state.buffer = surface->state.buffer;
    wl_list_init(&surface->pending.state.frame_callbacks);

    event.type = SWC_SURFACE_REPAINT;
    wl_signal_emit(&surface->event_signal, &event);
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
    surface->output_mask = 0;
    surface->geometry.x = 0;
    surface->geometry.y = 0;
    surface->geometry.width = 0;
    surface->geometry.height = 0;
    surface->border.width = 0;
    surface->border.color = 0x000000;

    state_initialize(&surface->state);
    state_initialize(&surface->pending.state);

    /* The input region should be intersected with the surface's geometry,
     * which at this point is empty. */
    pixman_region32_clear(&surface->state.input);

    wl_signal_init(&surface->event_signal);

    /* Add the surface to the client. */
    surface->resource
        = wl_client_add_object(client, &wl_surface_interface,
                               &surface_implementation, id, surface);
    wl_resource_set_destructor(surface->resource, &surface_destroy);

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

