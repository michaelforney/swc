#ifndef SWC_I915_BO_H
#define SWC_I915_BO_H 1

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

struct intel_bo
{
    uint32_t handle;
    uint32_t size;

    uint64_t last_offset;
};

bool intel_bo_initialize(int drm, struct intel_bo * bo, uint32_t size);

void intel_bo_finalize(int drm, struct intel_bo * bo);

void intel_bo_write(int drm, struct intel_bo * bo, uint32_t offset,
                   void * data, size_t size);

#endif

