#ifndef SWC_INTEL_MI_H
#define SWC_INTEL_MI_H 1

#include "batch.h"

#include <stdint.h>
#include <stdbool.h>

#define COMMAND_TYPE_MI 0x0

#define MI_OP(opcode) (                                                     \
      COMMAND_TYPE_MI << 29 /* 31:29 */                                     \
    | opcode << 23          /* 28:23 */                                     \
)

#define MI_NOOP                 MI_OP(0x00)
#define MI_FLUSH                MI_OP(0x04)
#define MI_BATCH_BUFFER_END     MI_OP(0x0A)

/* MI_NOOP */
#define MI_NOOP_IDENTIFICATION_NUMBER(number)       (1 << 22 | number)

/* MI_FLUSH */
#define MI_FLUSH_ENABLE_PROTECTED_MEMORY            (1 << 6)
#define MI_FLUSH_DISABLE_INDIRECT_STATE_POINTERS    (1 << 5)
#define MI_FLUSH_CLEAR_GENERIC_MEDIA_STATE          (1 << 3)
#define MI_FLUSH_RESET_GLOBAL_SNAPSHOT_COUNT        (1 << 3)
#define MI_FLUSH_INHIBIT_RENDER_CACHE_FLUSH         (1 << 2)
#define MI_FLUSH_INVALIDATE_STATE_INSTRUCTION_CACHE (1 << 2)

#endif

