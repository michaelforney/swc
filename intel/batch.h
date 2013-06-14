#ifndef SWC_I915_BATCH_H
#define SWC_I915_BATCH_H 1

#include "bo.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#include <libdrm/i915_drm.h>

#define I915_MAX_COMMANDS (1 << 15)
#define I915_MAX_RELOCATIONS (1 << 11)
#define I915_MAX_EXEC_OBJECTS (1 << 11)

struct intel_batch
{
    int drm;

    struct drm_i915_gem_relocation_entry relocations[I915_MAX_RELOCATIONS];
    uint64_t * offsets[I915_MAX_RELOCATIONS];
    uint32_t relocation_count;

    struct drm_i915_gem_exec_object2 exec_objects[I915_MAX_EXEC_OBJECTS];
    uint32_t exec_object_count;

    //uint32_t header[13];
    uint32_t commands[I915_MAX_COMMANDS];
    uint32_t command_count;
};

void intel_batch_initialize(struct intel_batch * batch, int drm);

void intel_batch_flush(struct intel_batch * batch);

void intel_batch_ensure_space(struct intel_batch * batch, uint32_t size);

uint32_t intel_batch_space(struct intel_batch * batch);

static inline void intel_batch_add_dword(struct intel_batch * batch,
                                        uint32_t dword)
{
    batch->commands[batch->command_count++] = dword;
}

static inline void intel_batch_add_dwords(struct intel_batch * batch, uint32_t count, ...)
{
    va_list dwords;
    va_start(dwords, count);
    while (count--)
        intel_batch_add_dword(batch, va_arg(dwords, int));
    va_end(dwords);
}

uint64_t intel_batch_add_relocation(struct intel_batch * batch,
                                   uint32_t batch_offset, struct intel_bo * bo,
                                   uint32_t read_domains, uint32_t write_domain);

void intel_batch_add_exec_object(struct intel_batch * batch, struct intel_bo * bo);

#endif

