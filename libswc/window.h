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
#include "pointer.h"

#include <stdint.h>
#include <wayland-server.h>

struct window_pointer_interaction
{
    struct pointer_handler handler, * original_handler;
};

struct window
{
    struct swc_window base;
    const struct window_impl * impl;

    struct compositor_view * view;

    struct
    {
        struct window_pointer_interaction interaction;
        struct { int32_t x, y; } offset;
    } move;

    struct
    {
        struct window_pointer_interaction interaction;
        uint32_t edges;
    } resize;
};

struct window_impl
{
    void (* configure)(struct window * window,
                       const struct swc_rectangle * geometry);
    void (* focus)(struct window * window);
};

extern struct wl_listener window_enter_listener;

bool window_initialize(struct window * window, const struct window_impl * impl,
                       struct swc_surface * surface);

void window_finalize(struct window * window);

void window_set_title(struct window * window,
                      const char * title, size_t length);

void window_set_class(struct window * window, const char * class);

void window_set_state(struct window * window, uint32_t state);

void window_set_parent(struct window * window, struct window * parent);

void window_begin_move(struct window * window, struct button * button);

void window_begin_resize(struct window * window, uint32_t edges,
                         struct button * button);

#endif

