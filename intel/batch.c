#include "batch.h"
#include "bo.h"
#include "mi.h"

#include <stdio.h>
#include <xf86drm.h>

void intel_batch_initialize(struct intel_batch * batch, drm_intel_bufmgr * bufmgr)
{
    batch->bufmgr = bufmgr;
    batch->command_count = 0;

    /* Alignment (4096) is not used */
    batch->bo = drm_intel_bo_alloc(bufmgr, "batchbuffer",
                                   sizeof batch->commands, 4096);
}

void intel_batch_finalize(struct intel_batch * batch)
{
    drm_intel_bo_unreference(batch->bo);
}

void intel_batch_flush(struct intel_batch * batch)
{
    if (batch->command_count == 0)
        return;

    intel_batch_add_dword(batch, MI_BATCH_BUFFER_END);

    /* Pad the batch buffer to the next quad-word. */
    if (batch->command_count & 1)
        intel_batch_add_dword(batch, MI_NOOP);

    drm_intel_bo_subdata(batch->bo, 0, batch->command_count << 2,
                         batch->commands);
    int ret = drm_intel_bo_exec(batch->bo, batch->command_count << 2, NULL, 0,
                                0);
    //printf("ret: %d\n", ret);
    drm_intel_gem_bo_clear_relocs(batch->bo, 0);

    batch->command_count = 0;
}

void intel_batch_ensure_space(struct intel_batch * batch, uint32_t size)
{
    if (intel_batch_space(batch) < size)
        intel_batch_flush(batch);
}

