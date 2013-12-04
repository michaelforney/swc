/* swc: libswc/window.h
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

#ifndef SWC_WINDOW_H
#define SWC_WINDOW_H

#include "swc.h"

#include <stdint.h>
#include <wayland-server.h>

struct swc_window_impl
{
    void (* configure)(struct swc_window * window,
                       const struct swc_rectangle * geometry);
    void (* focus)(struct swc_window * window);
};

struct swc_window_internal
{
    struct swc_window base;

    struct swc_surface * surface;
    const struct swc_window_impl * impl;
};

extern struct wl_listener * swc_window_enter_listener;

bool swc_window_initialize(struct swc_window * window,
                           const struct swc_window_impl * impl,
                           struct swc_surface * surface);

void swc_window_finalize(struct swc_window * window);

void swc_window_set_title(struct swc_window * window,
                          const char * title, size_t length);

void swc_window_set_class(struct swc_window * window, const char * class);

void swc_window_set_state(struct swc_window * window, uint32_t state);

#endif

