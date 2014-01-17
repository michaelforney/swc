/* swc: libswc/screen.h
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

#ifndef SWC_SCREEN_H
#define SWC_SCREEN_H

#include "swc.h"

#include <wayland-util.h>

struct swc_output;

struct swc_screen_internal
{
    struct swc_screen base;

    uint8_t id;

    struct wl_list outputs;
    struct wl_list modifiers;
    struct wl_list link;
};

bool swc_screens_initialize();
void swc_screens_finalize();

struct swc_screen_internal * swc_screen_new(uint32_t crtc,
                                            struct swc_output * output);
void swc_screen_destroy(struct swc_screen_internal * screen);
static inline uint32_t swc_screen_mask(struct swc_screen_internal * screen)
{
    return 1 << screen->id;
}

#endif

