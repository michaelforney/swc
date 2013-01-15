#include "renderer.h"
#include "i915/blt.h"
#include "i915/mi.h"

#include <stdio.h>
#include <GLES2/gl2.h>
#include <libdrm/intel_bufmgr.h>

struct wl_drm_buffer
{
    struct wl_buffer buffer;
    struct wl_drm * drm;
    uint32_t format;
    const void * driver_format;
    int32_t offset[3];
    int32_t stride[3];
    void * driver_buffer;
};

struct __DRIimageRec
{
    struct intel_region * region;
    GLenum internal_format;
    uint32_t dri_format;
    GLuint format;
    uint32_t offset;
    uint32_t strides[3];
    uint32_t offsets[3];
    struct intel_image_format * planar_format;
    void * data;
};

struct intel_region
{
    drm_intel_bo * bo;
    GLuint refcount;
    GLuint cpp;
    GLuint width;
    GLuint height;
    GLuint pitch;
    GLubyte * map;
    GLuint map_refcount;
    uint32_t tiling;
    uint32_t name;
    struct intel_screen * screen;
};

static inline uint32_t format_wayland_to_pixman(uint32_t wayland_format)
{
    switch (wayland_format)
    {
        case WL_SHM_FORMAT_XRGB8888:
            return PIXMAN_x8r8g8b8;
        case WL_SHM_FORMAT_ARGB8888:
            return PIXMAN_a8r8g8b8;
    }

    return 0;
}

static void repaint_surface_for_output(struct swc_renderer * renderer,
                                       struct swc_surface * surface,
                                       struct swc_output * output)
{
    struct swc_buffer * back_buffer = swc_output_get_back_buffer(output);

    if (wl_buffer_is_shm(surface->state.buffer))
    {
        printf("repainting shm surface\n");
        pixman_image_composite32(PIXMAN_OP_SRC,
                                 surface->renderer_state.shm.image, NULL,
                                 back_buffer->image,
                                 0, 0, 0, 0, 0, 0,
                                 surface->geometry.width,
                                 surface->geometry.height);
    }
    else
    {
        /*
        struct i915_bo * src = &surface->renderer_state.drm.bo;
        uint32_t src_pitch = surface->renderer_state.drm.pitch;

        xy_src_copy_blt(&renderer->batch, src, src_pitch, 0, 0,
                        &back_buffer->bo, back_buffer->pitch, 0, 0,
                        surface->geometry.width, surface->geometry.height);
        */
    }
}

bool swc_renderer_initialize(struct swc_renderer * renderer,
                             struct swc_drm * drm)
{
    renderer->drm = drm;

    i915_batch_initialize(&renderer->batch, drm->fd);

    return true;
}

void swc_renderer_finalize(struct swc_renderer * renderer)
{
}

void swc_renderer_repaint_output(struct swc_renderer * renderer,
                                 struct swc_output * output,
                                 struct wl_list * surfaces)
{
    struct swc_surface * surface;

    printf("repainting output %u\n", output->id);

    wl_list_for_each(surface, surfaces, link)
    {
        if (surface->output_mask & (1 << output->id))
        {
            repaint_surface_for_output(renderer, surface, output);
        }
    }

    xy_color_blt(&renderer->batch, &swc_output_get_back_buffer(output)->bo,
                 swc_output_get_back_buffer(output)->pitch, 0, 0, 500, 500,
                 0xffffffff);

    //mi_flush(&renderer->batch, false, false, false, false, false, false);

    i915_batch_flush(&renderer->batch);
}

void swc_renderer_attach(struct swc_renderer * renderer,
                         struct wl_list * outputs,
                         struct swc_surface * surface,
                         struct wl_buffer * buffer)
{
    if (wl_buffer_is_shm(buffer))
    {
        struct swc_output * output;
        uint32_t wayland_format = wl_shm_buffer_get_format(buffer);

        surface->renderer_state.shm.image
            = pixman_image_create_bits(format_wayland_to_pixman(wayland_format),
                                       wl_shm_buffer_get_width(buffer),
                                       wl_shm_buffer_get_height(buffer),
                                       wl_shm_buffer_get_data(buffer),
                                       wl_shm_buffer_get_stride(buffer));

        wl_list_for_each(output, outputs, link)
        {
            if (surface->output_mask & (1 << output->id))
            {
                swc_buffer_ref_image(&output->buffers[0], renderer->drm);
                swc_buffer_ref_image(&output->buffers[1], renderer->drm);
            }
        }
    }
    else
    {
        struct wl_drm_buffer * drm_buffer = (void *) surface->state.buffer;
        struct __DRIimageRec * image = drm_buffer->driver_buffer;
        struct intel_region * region = image->region;
        drm_intel_bo * bo = region->bo;

        surface->renderer_state.drm.bo = (struct i915_bo) {
            .handle = bo->handle
        };

        surface->renderer_state.drm.pitch = region->pitch;

        printf("buffer width: %u, height: %u\n", buffer->width, buffer->height);

        printf("bo width: %u, height: %u, stride: %u, handle: %u\n",
               region->width, region->height,
               region->pitch, bo->handle);
    }
}

