/* swc: swc/internal.h
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

#ifndef SWC_INTERNAL_H
#define SWC_INTERNAL_H

#include <wayland-server.h>
#include <stdbool.h>

enum
{
    SWC_EVENT_ACTIVATED,
    SWC_EVENT_DEACTIVATED,
};

struct swc
{
    struct wl_display * display;
    struct wl_event_loop * event_loop;
    const struct swc_manager * manager;
    struct wl_signal event_signal;
    bool active;

    const struct swc_seat * const seat;
    const struct swc_bindings * const bindings;
    struct wl_list screens;
    struct swc_compositor * const compositor;
    struct swc_shm * const shm;
    struct swc_drm * const drm;

#ifdef ENABLE_XWAYLAND
    const struct swc_xserver * const xserver;
#endif
};

extern struct swc swc;

void swc_activate(void);
void swc_deactivate(void);

#endif

