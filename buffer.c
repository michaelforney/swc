#include "buffer.h"

#include <stdio.h>
#include <sys/mman.h>
#include <libdrm/i915_drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/* Returns the next multiple of the eth power of 2 */
static inline uint32_t next_multiple_power_2(uint32_t n, uint32_t e)
{
    return (n + (1 << e) - 1) & ~((1 << e) - 1);
}

static void destroy_image(pixman_image_t * image, void * data)
{
    struct swc_buffer * buffer = data;

    munmap(pixman_image_get_data(image), buffer->bo.size);
}

bool swc_buffer_initialize(struct swc_buffer * buffer, struct swc_drm * drm,
                           uint32_t width, uint32_t height)
{
    uint32_t size;

    buffer->image = NULL;

    buffer->width = width;
    buffer->height = height;

    /* Round width up to next multiple of 512 */
    buffer->pitch = next_multiple_power_2(width * 4, 9);

    /* Round height up to next multiple of 4 */
    size = buffer->pitch * next_multiple_power_2(height, 2);

    printf("width: %u, height: %u, pitch: %u, size: %u\n", width, height, buffer->pitch, size);

    intel_bo_initialize(drm->fd, &buffer->bo, size);

    if (drmModeAddFB(drm->fd, width, height, 24, 32, buffer->pitch,
                     buffer->bo.handle, &buffer->id) != 0)
    {
        printf("could not create FB from buffer handle\n");
        goto error_buffer;
    }

    return true;

  error_buffer:
    {
        struct drm_gem_close close_arg = { .handle = buffer->bo.handle };
        drmIoctl(drm->fd, DRM_IOCTL_GEM_CLOSE, &close_arg);
    }
  error_base:
    return false;
}

void swc_buffer_finish(struct swc_buffer * buffer, struct swc_drm * drm)
{
    drmModeRmFB(drm->fd, buffer->id);
    intel_bo_finalize(drm->fd, &buffer->bo);
}

void swc_buffer_ref_image(struct swc_buffer * buffer, struct swc_drm * drm)
{
    if (!buffer->image)
    {
        uint32_t * data;
        struct drm_i915_gem_mmap mmap_arg = {
            .handle = buffer->bo.handle,
            .size = buffer->bo.size
        };

        if (drmCommandWriteRead(drm->fd, DRM_I915_GEM_MMAP, &mmap_arg,
                                sizeof mmap_arg) != 0)
        {
            printf("could not mmap buffer\n");
            return;
        }

        data = (void *) mmap_arg.addr_ptr;
        buffer->image = pixman_image_create_bits(PIXMAN_x8r8g8b8,
                                                 buffer->width, buffer->height,
                                                 data, buffer->pitch);
        pixman_image_set_destroy_function(buffer->image, &destroy_image, buffer);
    }
    else
        pixman_image_ref(buffer->image);
}

void swc_buffer_unref_image(struct swc_buffer * buffer)
{
    if (pixman_image_unref(buffer->image))
        buffer->image = NULL;
}

