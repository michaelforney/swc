#include "surface.h"
#include "event.h"
#include "region.h"

#include <stdio.h>

static void state_initialize(struct swc_surface_state * state)
{
    state->buffer = NULL;

    pixman_region32_init(&state->damage);
    pixman_region32_init(&state->opaque);
    pixman_region32_init(&state->input);

    wl_list_init(&state->frame_callbacks);
}

static void destroy_surface_resource(struct wl_resource * resource)
{
    struct swc_surface * surface = resource->data;

    swc_surface_finish(surface);
}

static void destroy(struct wl_client * client, struct wl_resource * resource)
{
    wl_resource_destroy(resource);
}

static void attach(struct wl_client * client, struct wl_resource * resource,
                   struct wl_resource * buffer_resource, int32_t x, int32_t y)
{
    struct swc_surface * surface = resource->data;

    printf("surface_attach\n");

    surface->pending.x = x;
    surface->pending.y = y;

    if (buffer_resource)
    {
        struct wl_buffer * buffer = buffer_resource->data;

        surface->pending.state.buffer = buffer;
        surface->geometry.width = buffer->width;
        surface->geometry.height = buffer->height;
    }
    else
    {
        surface->pending.state.buffer = NULL;

        surface->geometry.width = 0;
        surface->geometry.height = 0;
    }
}

static void damage(struct wl_client * client, struct wl_resource * resource,
                   int32_t x, int32_t y, int32_t width, int32_t height)
{
    printf("surface_damage\n");
    struct swc_surface * surface = resource->data;

    pixman_region32_union_rect(&surface->pending.state.damage,
                               &surface->pending.state.damage,
                               x, y, width, height);
}

static void frame(struct wl_client * client, struct wl_resource * resource,
                  uint32_t id)
{
    struct swc_surface * surface = resource->data;
    struct wl_resource * callback_resource;

    printf("surface_frame\n");

    callback_resource = wl_client_add_object(client, &wl_callback_interface,
                                             NULL, id, NULL);
    wl_list_insert(surface->pending.state.frame_callbacks.prev,
                   &callback_resource->link);
}

static void set_opaque_region(struct wl_client * client,
                              struct wl_resource * resource,
                              struct wl_resource * region_resource)
{
    struct swc_surface * surface = resource->data;

    printf("surface_set_opaque_region\n");

    if (region_resource)
    {
        struct swc_region * region = region_resource->data;

        pixman_region32_copy(&surface->pending.state.opaque, &region->region);
    }
    else
        pixman_region32_clear(&surface->pending.state.opaque);
}

static void set_input_region(struct wl_client * client,
                             struct wl_resource * resource,
                             struct wl_resource * region_resource)
{
    struct swc_surface * surface = resource->data;

    printf("surface_set_input_region\n");

    if (region_resource)
    {
        struct swc_region * region = region_resource->data;

        pixman_region32_copy(&surface->pending.state.input, &region->region);
    }
    else
        pixman_region32_clear(&surface->pending.state.input);
}

static void commit(struct wl_client * client, struct wl_resource * resource)
{
    struct swc_surface * surface = resource->data;
    struct swc_event event;

    printf("surface_commit\n");

    event.data = surface;

    if (surface->pending.state.buffer != surface->state.buffer)
    {
        surface->state.buffer = surface->pending.state.buffer;
        event.type = SWC_SURFACE_ATTACH;
        wl_signal_emit(&surface->event_signal, &event);
    }

    pixman_region32_union(&surface->state.damage, &surface->state.damage,
                          &surface->pending.state.damage);
    pixman_region32_intersect_rect(&surface->state.damage,
                                   &surface->state.damage, 0, 0,
                                   surface->geometry.width,
                                   surface->geometry.height);


    wl_list_insert_list(&surface->state.frame_callbacks,
                        &surface->pending.state.frame_callbacks);

    /* Reset pending state */
    pixman_region32_clear(&surface->pending.state.damage);
    pixman_region32_clear(&surface->pending.state.opaque);
    pixman_region32_clear(&surface->pending.state.input);
    surface->pending.state.buffer = surface->state.buffer;
    wl_list_init(&surface->pending.state.frame_callbacks);

    event.type = SWC_SURFACE_REPAINT;
    wl_signal_emit(&surface->event_signal, &event);
}

struct wl_surface_interface surface_implementation = {
    .destroy = &destroy,
    .attach = &attach,
    .damage = &damage,
    .frame = &frame,
    .set_opaque_region = &set_opaque_region,
    .set_input_region = &set_input_region,
    .commit = &commit,
};

bool swc_surface_initialize(struct swc_surface * surface,
                            struct wl_client * client, uint32_t id)
{
    state_initialize(&surface->state);
    state_initialize(&surface->pending.state);

    surface->resource = wl_client_add_object(client, &wl_surface_interface,
                                             &surface_implementation, id, surface);
    wl_resource_set_destructor(surface->resource, &destroy_surface_resource);

    wl_signal_init(&surface->event_signal);

    surface->output_mask = 0;
    surface->geometry.x = 0;
    surface->geometry.y = 0;
    surface->geometry.width = 0;
    surface->geometry.height = 0;
    surface->border.width = 0;
    surface->border.color = 0x000000;

    return true;
}

void swc_surface_finish(struct swc_surface * surface)
{
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

