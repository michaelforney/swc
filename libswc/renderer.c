#include "renderer.h"
#include "compositor_surface.h"
#include "drm_buffer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <wld/wld.h>
#include <wld/drm.h>

struct buffer_state
{
    struct wld_drawable * drawable;
    /* Only used for SHM buffers */
    pixman_image_t * dst, * src;
    struct wl_listener destroy_listener;
};

static inline uint32_t pixman_format(uint32_t format)
{
    switch (format)
    {
        case WL_SHM_FORMAT_XRGB8888:
            return PIXMAN_x8r8g8b8;
        case WL_SHM_FORMAT_ARGB8888:
            return PIXMAN_a8r8g8b8;
    }

    return 0;
}

static inline uint32_t wld_format(uint32_t format)
{
    switch (format)
    {
        case WL_SHM_FORMAT_XRGB8888:
            return WLD_FORMAT_XRGB8888;
        case WL_SHM_FORMAT_ARGB8888:
            return WLD_FORMAT_ARGB8888;
    }

    return 0;
}

static void handle_buffer_destroy(struct wl_listener * listener, void * data)
{
    struct buffer_state * state
        = CONTAINER_OF(listener, typeof(*state), destroy_listener);

    if (state->dst)
        pixman_image_unref(state->dst);
    if (state->src)
        pixman_image_unref(state->src);
    wld_destroy_drawable(state->drawable);
    free(state);
}

static inline struct buffer_state * buffer_state(struct wl_resource * resource)
{
    struct wl_listener * listener
        = wl_resource_get_destroy_listener(resource, &handle_buffer_destroy);

    return listener ? CONTAINER_OF(listener, struct buffer_state,
                                   destroy_listener)
                    : NULL;
}

static void repaint_surface(struct swc_renderer * renderer,
                            struct swc_surface * surface,
                            pixman_region32_t * damage)
{
    struct swc_render_target * target = &renderer->target;
    pixman_region32_t surface_damage;
    pixman_region32_t border_damage;
    pixman_region32_t surface_region;
    struct buffer_state * state;
    struct swc_compositor_surface_state * surface_state = surface->class_state;

    if (!surface->state.buffer)
        return;

    state = buffer_state(surface->state.buffer);
    assert(state);

    pixman_region32_init_with_extents(&surface_damage, &surface_state->extents);
    pixman_region32_init(&border_damage);
    pixman_region32_init_rect
        (&surface_region, surface->geometry.x, surface->geometry.y,
         surface->geometry.width, surface->geometry.height);

    pixman_region32_intersect(&surface_damage, damage, &surface_damage);
    pixman_region32_subtract(&surface_damage, &surface_damage,
                             &surface_state->clip);
    pixman_region32_subtract(&border_damage, &surface_damage, &surface_region);
    pixman_region32_intersect(&surface_damage, &surface_damage,
                              &surface_region);

    pixman_region32_fini(&surface_region);

    if (pixman_region32_not_empty(&surface_damage))
    {
        printf("\tdrm surface %u { x: %d, y: %d, w: %u, h: %u }\n",
               wl_resource_get_id(surface->resource),
               surface->geometry.x, surface->geometry.y,
               surface->geometry.width, surface->geometry.height);

        pixman_region32_translate(&surface_damage,
                                  -surface->geometry.x, -surface->geometry.y);
        wld_copy_region(state->drawable, target->drawable, &surface_damage,
                        surface->geometry.x - target->geometry.x,
                        surface->geometry.y - target->geometry.y);
    }

    pixman_region32_fini(&surface_damage);

    /* Draw border */
    if (pixman_region32_not_empty(&border_damage))
    {
        printf("\tborder\n");

        pixman_region32_translate(&border_damage,
                                  -target->geometry.x, -target->geometry.y);
        wld_fill_region(target->drawable, surface_state->border.color,
                        &border_damage);
    }

    pixman_region32_fini(&border_damage);
}

bool swc_renderer_initialize(struct swc_renderer * renderer,
                             struct swc_drm * drm)
{
    renderer->drm = drm;

    return true;
}

void swc_renderer_finalize(struct swc_renderer * renderer)
{
}

void swc_renderer_set_target(struct swc_renderer * renderer,
                             struct swc_plane * plane)
{
    struct wld_drawable * drawable = swc_plane_get_buffer(plane);

    renderer->target.drawable = drawable;
    renderer->target.geometry.x = plane->output->geometry.x + plane->x;
    renderer->target.geometry.y = plane->output->geometry.y + plane->y;
    renderer->target.geometry.width = drawable->width;
    renderer->target.geometry.height = drawable->height;
}

void swc_renderer_repaint(struct swc_renderer * renderer,
                          pixman_region32_t * damage,
                          pixman_region32_t * base_damage,
                          struct wl_list * surfaces)
{
    struct swc_render_target * target = &renderer->target;
    struct swc_surface * surface;

    printf("rendering to target { x: %d, y: %d, w: %u, h: %u }\n",
           target->geometry.x, target->geometry.y,
           target->geometry.width, target->geometry.height);

    /* Paint base damage black. */
    if (pixman_region32_not_empty(base_damage))
    {
        pixman_region32_translate(base_damage,
                                  -target->geometry.x, -target->geometry.y);
        wld_fill_region(target->drawable, 0xff000000, base_damage);
    }

    wl_list_for_each_reverse(surface, surfaces, link)
    {
        if (swc_rectangle_overlap(&target->geometry, &surface->geometry))
            repaint_surface(renderer, surface, damage);
    }

    wld_flush(target->drawable);
}

void swc_renderer_attach(struct swc_renderer * renderer,
                         struct swc_surface * surface,
                         struct wl_resource * resource)
{
    struct buffer_state * state;
    struct wl_shm_buffer * shm_buffer;
    struct swc_drm_buffer * drm_buffer;

    if (!resource)
        return;

    /* Check if we have already seen this buffer. */
    if ((state = buffer_state(resource)))
        return;

    if (!(state = malloc(sizeof *state)))
        return;

    if ((shm_buffer = wl_shm_buffer_get(resource)))
    {
        uint32_t width = wl_shm_buffer_get_width(shm_buffer),
                 height = wl_shm_buffer_get_height(shm_buffer),
                 format = wl_shm_buffer_get_format(shm_buffer),
                 pitch = wl_shm_buffer_get_stride(shm_buffer);
        void * data = wl_shm_buffer_get_data(shm_buffer);

        state->drawable = wld_drm_create_drawable(renderer->drm->context,
                                                  width, height,
                                                  wld_format(format));
        state->src = pixman_image_create_bits_no_clear(pixman_format(format),
                                                       width, height,
                                                       data, pitch);
        state->dst = wld_map(state->drawable);
    }
    else if ((drm_buffer = swc_drm_buffer_get(resource)))
    {
        if (!(state = malloc(sizeof *state)))
            return;

        state->drawable = drm_buffer->drawable;
        state->src = NULL;
        state->dst = NULL;
    }
    else
    {
        fprintf(stderr, "Unsupported buffer type\n");
        return;
    }

    state->destroy_listener.notify = &handle_buffer_destroy;
    wl_resource_add_destroy_listener(resource, &state->destroy_listener);
}

void swc_renderer_flush(struct swc_renderer * renderer,
                        struct swc_surface * surface)
{
    struct buffer_state * state;

    state = buffer_state(surface->state.buffer);
    assert(state);

    if (!state->src || !state->dst)
        return;

    pixman_image_set_clip_region32(state->src, &surface->state.damage);
    pixman_image_composite32(PIXMAN_OP_SRC, state->src, NULL, state->dst,
                             0, 0, 0, 0, 0, 0,
                             state->drawable->width, state->drawable->height);
}

