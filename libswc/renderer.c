#include "renderer.h"
#include "compositor_surface.h"
#include "util.h"

#include <stdio.h>
#include <GLES2/gl2.h>
#include <libdrm/intel_bufmgr.h>
#include <libdrm/drm.h>
#include <intelbatch/blt.h>
#include <intelbatch/mi.h>
#include <xf86drm.h>

struct buffer_state
{
    union
    {
        struct
        {
            pixman_image_t * image;
        } shm;
        struct
        {
            drm_intel_bo * bo;
            uint32_t width, height, pitch;
        } drm;
    };
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

static inline void switch_context(struct swc_renderer * renderer,
                                  uint32_t context, struct swc_buffer * buffer)
{
    if (renderer->context != context)
    {
        /* Leave old context */
        switch (renderer->context)
        {
            case SWC_RENDERER_CONTEXT_NONE:
                break;
            case SWC_RENDERER_CONTEXT_BATCH:
                intel_batch_flush(&renderer->batch);
                break;
            case SWC_RENDERER_CONTEXT_SHM:
                drm_intel_gem_bo_unmap_gtt(buffer->bo);
                break;
        }

        /* Enter new context */
        switch (context)
        {
            case SWC_RENDERER_CONTEXT_NONE:
                break;
            case SWC_RENDERER_CONTEXT_BATCH:
                break;
            case SWC_RENDERER_CONTEXT_SHM:
                drm_intel_gem_bo_map_gtt(buffer->bo);
                break;
        }

        renderer->context = context;
    }
}

static void handle_buffer_destroy(struct wl_listener * listener, void * data)
{
    struct buffer_state * state
        = swc_container_of(listener, typeof(*state), destroy_listener);

    free(state);
}

static inline struct buffer_state * buffer_state(struct wl_resource * resource)
{
    struct wl_listener * listener
        = wl_resource_get_destroy_listener(resource, &handle_buffer_destroy);

    return listener ? swc_container_of(listener, struct buffer_state,
                                       destroy_listener)
                    : NULL;
}

static void repaint_surface_for_output(struct swc_renderer * renderer,
                                       struct swc_surface * surface,
                                       struct swc_output * output)
{
    struct swc_buffer * back_buffer = swc_output_get_back_buffer(output);
    struct buffer_state * state;
    struct swc_compositor_surface_state * surface_state = surface->class_state;

    if (!surface->state.buffer)
        return;

    state = buffer_state(&surface->state.buffer->resource);

    if (wl_buffer_is_shm(surface->state.buffer))
    {
        pixman_image_t * buffer_image;

        switch_context(renderer, SWC_RENDERER_CONTEXT_SHM, back_buffer);

        printf("repainting shm surface\n");

        buffer_image = pixman_image_create_bits_no_clear
            (PIXMAN_x8r8g8b8, back_buffer->width, back_buffer->height,
             back_buffer->bo->virtual, back_buffer->pitch);

        pixman_image_composite32(PIXMAN_OP_SRC,
                                 state->shm.image, NULL,
                                 buffer_image, 0, 0, 0, 0, 0, 0,
                                 surface->geometry.width,
                                 surface->geometry.height);
    }
    else
    {
        switch_context(renderer, SWC_RENDERER_CONTEXT_BATCH, back_buffer);

        printf("repainting drm surface\n");

        drm_intel_bo * src = state->drm.bo;
        uint32_t src_pitch = state->drm.pitch;

        xy_src_copy_blt(&renderer->batch, src, src_pitch, 0, 0,
                        back_buffer->bo, back_buffer->pitch,
                        surface->geometry.x + surface_state->border.width,
                        surface->geometry.y + surface_state->border.width,
                        surface->geometry.width, surface->geometry.height);
    }

    /* Draw border */
    {
        switch_context(renderer, SWC_RENDERER_CONTEXT_BATCH, back_buffer);

        /* Top */
        xy_color_blt(&renderer->batch, back_buffer->bo, back_buffer->pitch,
                     surface->geometry.x, surface->geometry.y,
                     surface->geometry.x + surface->geometry.width + 2 * surface_state->border.width,
                     surface->geometry.y + surface_state->border.width,
                     surface_state->border.color);
        /* Bottom */
        xy_color_blt(&renderer->batch, back_buffer->bo, back_buffer->pitch,
                     surface->geometry.x,
                     surface->geometry.y + surface_state->border.width + surface->geometry.height,
                     surface->geometry.x + surface->geometry.width + 2 * surface_state->border.width,
                     surface->geometry.y + surface->geometry.height + 2 * surface_state->border.width,
                     surface_state->border.color);
        /* Left */
        xy_color_blt(&renderer->batch, back_buffer->bo, back_buffer->pitch,
                     surface->geometry.x, surface->geometry.y + surface_state->border.width,
                     surface->geometry.x + surface_state->border.width,
                     surface->geometry.y + + surface_state->border.width + surface->geometry.height,
                     surface_state->border.color);
        /* Right */
        xy_color_blt(&renderer->batch, back_buffer->bo, back_buffer->pitch,
                     surface->geometry.x + surface_state->border.width + surface->geometry.width,
                     surface->geometry.y + surface_state->border.width,
                     surface->geometry.x + surface->geometry.width + 2 * surface_state->border.width,
                     surface->geometry.y + surface_state->border.width + surface->geometry.height,
                     surface_state->border.color);

    }
}

bool swc_renderer_initialize(struct swc_renderer * renderer,
                             struct swc_drm * drm,
                             struct gbm_device * gbm)
{
    renderer->drm = drm;
    renderer->gbm = gbm;

    intel_batch_initialize(&renderer->batch, drm->bufmgr);

    return true;
}

void swc_renderer_finalize(struct swc_renderer * renderer)
{
    intel_batch_finalize(&renderer->batch);
}

void swc_renderer_repaint_output(struct swc_renderer * renderer,
                                 struct swc_output * output,
                                 struct wl_list * surfaces)
{
    struct swc_surface * surface;
    struct swc_buffer * back_buffer;

    back_buffer = swc_output_get_back_buffer(output);

    switch_context(renderer, SWC_RENDERER_CONTEXT_BATCH, back_buffer);

    wl_list_for_each(surface, surfaces, link)
    {
        if (surface->outputs & (1 << output->id))
            repaint_surface_for_output(renderer, surface, output);
    }

    switch_context(renderer, SWC_RENDERER_CONTEXT_NONE, back_buffer);
}

void swc_renderer_attach(struct swc_renderer * renderer,
                         struct swc_surface * surface,
                         struct wl_buffer * buffer)
{
    struct buffer_state * state;
    struct gbm_bo * bo;

    if (!buffer)
        return;

    /* Check if we have already seen this buffer. */
    if ((state = buffer_state(&buffer->resource)))
        return;

    if (!(state = malloc(sizeof *state)))
        return;

    /* SHM buffer */
    if (wl_buffer_is_shm(buffer))
    {
        struct swc_output * output;
        uint32_t wayland_format = wl_shm_buffer_get_format(buffer);

        state->shm.image
            = pixman_image_create_bits(pixman_format(wayland_format),
                                       wl_shm_buffer_get_width(buffer),
                                       wl_shm_buffer_get_height(buffer),
                                       wl_shm_buffer_get_data(buffer),
                                       wl_shm_buffer_get_stride(buffer));
    }
    /* DRM buffer */
    else if ((bo = gbm_bo_import(renderer->gbm, GBM_BO_IMPORT_WL_BUFFER, buffer,
                                 GBM_BO_USE_RENDERING)))
    {
        int handle = gbm_bo_get_handle(bo).s32;
        struct drm_gem_flink flink = { .handle = handle };

        if (drmIoctl(renderer->drm->fd, DRM_IOCTL_GEM_FLINK, &flink) != 0)
        {
            printf("could not flink handle\n");
            return;
        }

        state->drm.bo
            = drm_intel_bo_gem_create_from_name(renderer->drm->bufmgr,
                                                "surface", flink.name);
        state->drm.pitch = gbm_bo_get_stride(bo);
        state->drm.width = gbm_bo_get_width(bo);
        state->drm.height = gbm_bo_get_height(bo);

        printf("pitch: %u, width: %u, height: %u\n", state->drm.pitch,
            state->drm.width, state->drm.height);
    }
}

