#include "buffer.h"

#include <stdio.h>
#include <sys/mman.h>
#include <libdrm/i915_drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

bool swc_buffer_initialize(struct swc_buffer * buffer, struct swc_drm * drm,
                           uint32_t width, uint32_t height)
{
    uint32_t size;
    uint32_t tiling_mode = I915_TILING_X;
    unsigned long pitch;

    buffer->width = width;
    buffer->height = height;

    buffer->bo = drm_intel_bo_alloc_tiled(drm->bufmgr, "fb", width, height, 4,
                                          &tiling_mode, &pitch, 0);

    buffer->pitch = pitch;

    if (drmModeAddFB(drm->fd, width, height, 24, 32, buffer->pitch,
                     buffer->bo->handle, &buffer->id) != 0)
    {
        printf("could not create FB from buffer handle\n");
        goto error_buffer;
    }

    return true;

  error_buffer:
    drm_intel_bo_unreference(buffer->bo);
  error_base:
    return false;
}

void swc_buffer_finish(struct swc_buffer * buffer, struct swc_drm * drm)
{
    drmModeRmFB(drm->fd, buffer->id);
    drm_intel_bo_unreference(buffer->bo);
}

