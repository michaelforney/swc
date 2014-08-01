/* swc: libswc/xkb.h
 *
 * Copyright (c) 2013, 2014 Michael Forney
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

#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

#ifndef SWC_XKB_H
#define SWC_XKB_H

/* Keycodes are offset by 8 in XKB. */
#define XKB_KEY(key) ((key) + 8)

struct xkb
{
    struct xkb_context * context;
    struct xkb_state * state;

    struct
    {
        struct xkb_keymap * map;
        int fd;
        uint32_t size;
        char * area;
    } keymap;

    struct
    {
        uint32_t ctrl, alt, super, shift;
    } indices;
};

bool xkb_initialize(struct xkb * xkb);
void xkb_finalize(struct xkb * xkb);

bool xkb_update_keymap(struct xkb * xkb);

#endif

