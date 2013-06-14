#ifndef SWC_I915_MI_H
#define SWC_I915_MI_H 1

#include "batch.h"

#include <stdint.h>
#include <stdbool.h>

#define COMMAND_TYPE_MI 0x0

#define MI_OPCODE_NOOP                  0x00
#define MI_OPCODE_FLUSH                 0x04
#define MI_OPCODE_BATCH_BUFFER_END      0x0A
#define MI_OPCODE_FLUSH_DW              0x04
#define MI_OPCODE_BATCH_BUFFER_START    0x31

static inline void mi_noop(struct intel_batch * batch,
                           bool identification_number_write_enable,
                           uint32_t identification_number)
{
    intel_batch_add_dword(batch,
        COMMAND_TYPE_MI << 29                           /* 31:29 */
            | MI_OPCODE_NOOP << 23                      /* 28:23 */
            | identification_number_write_enable << 22  /* 22 */
            | identification_number << 0                /* 21:0 */
    );
}

static inline void mi_flush(struct intel_batch * batch,
                            bool protected_memory_enable,
                            bool indirect_state_pointers_disable,
                            bool generic_media_state_clear,
                            bool global_snapshot_count_reset,
                            bool render_cache_flush_inhibit,
                            bool state_cache_invalidate)
{
    intel_batch_add_dword(batch,
        COMMAND_TYPE_MI << 29                       /* 31:29 */
            | MI_OPCODE_FLUSH << 23                 /* 28:23 */
                                                    /* 22:7 */
            | protected_memory_enable << 6          /* 6 */
            | indirect_state_pointers_disable << 5  /* 5 */
            | generic_media_state_clear << 4        /* 4 */
            | global_snapshot_count_reset << 3      /* 3 */
            | render_cache_flush_inhibit << 2       /* 2 */
            | state_cache_invalidate                /* 1 */
                                                    /* 0 */
    );
}

static inline void mi_flush_dw(struct intel_batch * batch)
{
    intel_batch_add_dwords(batch, 4,
        COMMAND_TYPE_MI << 29
            | MI_OPCODE_FLUSH_DW << 23
            | 2
            ,
        0,
        0,
        0
    );
}

static inline void mi_batch_buffer_end(struct intel_batch * batch)
{
    /* XXX: semaphore data dword / semaphore address */
    intel_batch_add_dword(batch,
        COMMAND_TYPE_MI << 29                   /* 31:29 */
            | MI_OPCODE_BATCH_BUFFER_END << 23  /* 28:23 */
                                                /* 22:0 */
    );
}

static inline void mi_batch_buffer_start(struct intel_batch * batch,
                                         bool encrypted_memory_enable,
                                         bool clear_command_buffer_enable,
                                         bool buffer_non_secure,
                                         uint32_t buffer_address)
{
    intel_batch_ensure_space(batch, 2);

    intel_batch_add_dwords(batch, 2,
        COMMAND_TYPE_MI << 29                       /* 31:29 */
            | MI_OPCODE_BATCH_BUFFER_START << 23    /* 28:23 */
                                                    /* 22:13 */
            | encrypted_memory_enable << 12         /* 12 */
            | clear_command_buffer_enable << 11     /* 11 */
                                                    /* 10:9 */
            | buffer_non_secure << 8                /* 8 */
            | 0                                     /* 7:0 */
            ,
        buffer_address
    );
}

#endif

