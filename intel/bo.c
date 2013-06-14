#include "bo.h"

#include <stdio.h>
#include <xf86drm.h>
#include <libdrm/i915_drm.h>

bool intel_bo_initialize(int drm, struct intel_bo * bo, uint32_t size)
{
    struct drm_i915_gem_create create_arg = { .size = size };

    if (drmCommandWriteRead(drm, DRM_I915_GEM_CREATE, &create_arg,
                            sizeof create_arg) != 0)
    {
        printf("could not create bo\n");
        goto error_base;
    }

    bo->handle = create_arg.handle;
    bo->size = size;

    bo->last_offset = 0;

    return true;

  error_base:
    return false;
}

void intel_bo_finalize(int drm, struct intel_bo * bo)
{
    struct drm_gem_close close_arg = { .handle = bo->handle };

    drmIoctl(drm, DRM_IOCTL_GEM_CLOSE, &close_arg);
}

void intel_bo_write(int drm, struct intel_bo * bo, uint32_t offset,
                   void * data, size_t size)
{
    struct drm_i915_gem_pwrite pwrite_arg = {
        .handle = bo->handle,
        .offset = offset,
        .size = size,
        .data_ptr = (uint64_t) data
    };

    if (drmCommandWrite(drm, DRM_I915_GEM_PWRITE, &pwrite_arg,
                        sizeof pwrite_arg) != 0)
    {
        printf("write failed\n");
    }
}

