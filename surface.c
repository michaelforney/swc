#include "surface.h"
#include "event.h"

#include <stdio.h>

struct wl_surface_interface swc_surface_interface = {
    .destroy = &swc_surface_destroy,
    .attach = &swc_surface_attach,
    .damage = &swc_surface_damage,
    .frame = &swc_surface_frame,
    .set_opaque_region = &swc_surface_set_opaque_region,
    .set_input_region = &swc_surface_set_input_region,
    .commit = &swc_surface_commit,
};

static void state_initialize(struct swc_surface_state * state)
{
    state->buffer = NULL;

    pixman_region32_init(&state->damage);
    pixman_region32_init(&state->opaque);
    pixman_region32_init(&state->input);

    wl_list_init(&state->frame_callbacks);
}

bool swc_surface_initialize(struct swc_surface * surface)
{
    state_initialize(&surface->state);
    state_initialize(&surface->pending.state);

    wl_signal_init(&surface->event_signal);

    surface->output_mask = 0;

    return true;
}

void swc_surface_finish(struct swc_surface * surface)
{
}

void swc_surface_destroy(struct wl_client * client,
                         struct wl_resource * resource)
{
    wl_resource_destroy(resource);
}

void swc_surface_attach(struct wl_client * client,
                        struct wl_resource * resource,
                        struct wl_resource * buffer_resource,
                        int32_t x,
                        int32_t y)
{
    struct swc_surface * surface = resource->data;
    struct wl_buffer * buffer = buffer_resource->data;

    printf("surface_attach\n");

    surface->pending.state.buffer = buffer;
    surface->pending.x = x;
    surface->pending.y = y;

    surface->geometry.width = buffer->width;
    surface->geometry.height = buffer->height;
}

void swc_surface_damage(struct wl_client * client,
                        struct wl_resource * resource,
                        int32_t x, int32_t y,
                        int32_t width, int32_t height)
{
    printf("surface_damage\n");
    struct swc_surface * surface = resource->data;

    pixman_region32_union_rect(&surface->pending.state.damage,
                               &surface->pending.state.damage,
                               x, y, width, height);
}

void swc_surface_frame(struct wl_client * client,
                       struct wl_resource * resource,
                       uint32_t id)
{
    struct swc_surface * surface = resource->data;
    struct wl_resource * callback_resource;

    printf("surface_frame\n");

    callback_resource = wl_client_add_object(client, &wl_callback_interface,
                                             NULL, id, NULL);
    wl_list_insert(surface->pending.state.frame_callbacks.prev, &resource->link);
}

void swc_surface_set_opaque_region(struct wl_client * client,
                                   struct wl_resource * resource,
                                   struct wl_resource * region_resource)
{
    struct swc_surface * surface = resource->data;

    printf("surface_set_opaque_region\n");

    if (region_resource)
    {
    }
    else
    {
    }
}

void swc_surface_set_input_region(struct wl_client * client,
                                  struct wl_resource * resource,
                                  struct wl_resource * region)
{
    printf("surface_set_input_region\n");
}

void swc_surface_commit(struct wl_client * client,
                        struct wl_resource * resource)
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
    surface->pending.state.buffer = surface->state.buffer;
    wl_list_init(&surface->pending.state.frame_callbacks);

    event.type = SWC_SURFACE_REPAINT;
    wl_signal_emit(&surface->event_signal, &event);
}

