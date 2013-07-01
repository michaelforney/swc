/* swc: intel/batch.h
 *
 * Copyright (c) 2013 Michael Forney
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SWC_INTEL_BATCH_H
#define SWC_INTEL_BATCH_H 1

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#include <intel_bufmgr.h>

#define INTEL_MAX_COMMANDS (1 << 13)

struct intel_batch
{
    drm_intel_bufmgr * bufmgr;
    drm_intel_bo * bo;

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

static inline void intel_batch_add_dwords(struct intel_batch * batch,
                                          uint32_t count, ...)
{
    va_list dwords;
    va_start(dwords, count);
    while (count--)
        intel_batch_add_dword(batch, va_arg(dwords, uint32_t));
    va_end(dwords);
}

static inline uint32_t intel_batch_offset(struct intel_batch * batch,
                                         uint32_t command_index)
{
    return (batch->command_count + command_index) << 2;
}

#endif

