#ifndef SWC_INTEL_BATCH_H
#define SWC_INTEL_BATCH_H 1

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#include <libdrm/intel_bufmgr.h>

#define INTEL_MAX_COMMANDS (1 << 13)

struct intel_batch
{
    drm_intel_bufmgr * bufmgr;
    drm_intel_bo * bo;

    //uint32_t header[13];
    uint32_t commands[INTEL_MAX_COMMANDS];
    uint32_t command_count;
};

void intel_batch_initialize(struct intel_batch * batch, drm_intel_bufmgr * bufmgr);

void intel_batch_finalize(struct intel_batch * batch);

void intel_batch_flush(struct intel_batch * batch);

void intel_batch_ensure_space(struct intel_batch * batch, uint32_t size);

static inline uint32_t intel_batch_space(struct intel_batch * batch)
{
    /* XXX: reserved space */
    return INTEL_MAX_COMMANDS - batch->command_count;
}


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

static inline uint32_t intel_batch_offset(struct intel_batch * batch,
                                         uint32_t command_index)
{
    //printf("intel_batch_offset(4): %u\n", (batch->command_count + command_index) << 2);
    return (batch->command_count + command_index) << 2;
}

#endif

