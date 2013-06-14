#include "batch.h"
#include "bo.h"
#include "mi.h"

#include <stdio.h>
#include <xf86drm.h>

void intel_batch_initialize(struct intel_batch * batch, int drm)
{
    batch->relocation_count = 0;
    batch->exec_object_count = 0;
    batch->command_count = 0;
    batch->drm = drm;
}

void intel_batch_flush(struct intel_batch * batch)
{
    struct intel_bo bo;
    uint32_t index = batch->exec_object_count++;

    mi_batch_buffer_end(batch);

    /* Pad the batch buffer to the next quad-word. */
    if (batch->command_count & 1)
        mi_noop(batch, false, 0);

    printf("command count: %u\n", batch->command_count);

    intel_bo_initialize(batch->drm, &bo, batch->command_count << 2);
    intel_bo_write(batch->drm, &bo, 0, batch->commands, batch->command_count << 2);

    printf("adding exec object with handle: %u\n", bo.handle);

    /* Add command buffer */
    batch->exec_objects[index] = (struct drm_i915_gem_exec_object2) {
        .handle = bo.handle,
        .relocation_count = batch->relocation_count,
        .relocs_ptr = (uint64_t) batch->relocations
    };

    {
        int ret;
        struct drm_i915_gem_execbuffer2 execbuffer_arg = {
            .buffers_ptr = (uint64_t) batch->exec_objects,
            .buffer_count = batch->exec_object_count,
            .batch_start_offset = 0, /* XXX: ? */
            .batch_len = batch->command_count << 2,
            .flags = I915_EXEC_RENDER
        };

        if ((ret = drmIoctl(batch->drm, DRM_IOCTL_I915_GEM_EXECBUFFER2,
                     &execbuffer_arg)) != 0)
        {
            printf("execbuffer failed: %u\n", -ret);
        }
    }

    intel_bo_finalize(batch->drm, &bo);

    /* Set offsets for all our execution objects (except the last one, our
     * command object). */
    for (index = 0; index < batch->exec_object_count - 1; ++index)
        *batch->offsets[index] = batch->exec_objects[index].offset;

    batch->command_count = 0;
    batch->relocation_count = 0;
    batch->exec_object_count = 0;
}

#if 0
uint32_t * intel_batch_alloc(struct intel_batch * batch, uint32_t size)
{
    uint32_t * commands;

    if (intel_batch_space(batch) < size)
        intel_batch_flush(batch);

    commands = &batch->commands[batch->size];
    batch->command_count += command_count;

    return commands;
}
#endif

void intel_batch_ensure_space(struct intel_batch * batch, uint32_t size)
{
    if (intel_batch_space(batch) < size)
        intel_batch_flush(batch);
}

uint32_t intel_batch_space(struct intel_batch * batch)
{
    /* XXX: reserved space */
    return I915_MAX_COMMANDS - batch->command_count;
}

uint64_t intel_batch_add_relocation(struct intel_batch * batch,
                                   uint32_t batch_offset, struct intel_bo * bo,
                                   uint32_t read_domains, uint32_t write_domain)
{
    uint32_t index = batch->relocation_count++;

    intel_batch_add_exec_object(batch, bo);

    printf("offset: %u\n", (batch->command_count + batch_offset) << 2);
    printf("current: %u\n", *((uint32_t *)(((void *) batch->commands) + ((batch->command_count + batch_offset) << 2))));

    batch->relocations[index] = (struct drm_i915_gem_relocation_entry) {
        .target_handle = bo->handle,
        /* XXX: delta */
        /* XXX: offset */
        .offset = (batch->command_count + batch_offset) << 2,
        .presumed_offset = bo->last_offset,
        .read_domains = read_domains,
        .write_domain = write_domain
    };

    /* Return our offset guess */
    return bo->last_offset;
}

void intel_batch_add_exec_object(struct intel_batch * batch, struct intel_bo * bo)
{
    uint32_t index = batch->exec_object_count++;

    printf("adding exec object with handle: %u\n", bo->handle);

    batch->exec_objects[index] = (struct drm_i915_gem_exec_object2) {
        .handle = bo->handle
    };

    batch->offsets[index] = &bo->last_offset;
}

