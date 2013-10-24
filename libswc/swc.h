/* swc: libswc/swc.h
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

#ifndef SWC_H
#define SWC_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-server.h>

/* Windows {{{ */
enum
{
    SWC_WINDOW_DESTROYED,
    SWC_WINDOW_TITLE_CHANGED,
    SWC_WINDOW_CLASS_CHANGED,
    SWC_WINDOW_STATE_CHANGED,
    SWC_WINDOW_ENTERED,
    SWC_WINDOW_RESIZED
};

struct swc_window
{
    struct wl_signal event_signal;

    char * title;
    char * class;

    enum
    {
        SWC_WINDOW_STATE_WITHDRAWN,
        SWC_WINDOW_STATE_TOPLEVEL
    } state;
};

struct swc_window_manager
{
    void (* new_window)(struct swc_window * window);
};

void swc_window_show(struct swc_window * window);
void swc_window_hide(struct swc_window * window);
void swc_window_focus(struct swc_window * window);
void swc_window_set_geometry(struct swc_window * window, int32_t x, int32_t y,
                             uint32_t width, uint32_t height);
void swc_window_set_border(struct swc_window * window,
                           uint32_t color, uint32_t width);
/* }}} */

/* Events {{{ */
struct swc_event
{
    uint32_t type;
    void * data;
};
/* }}} */

bool swc_initialize(struct wl_display * display,
                    const struct swc_window_manager * window_manager);
void swc_finalize();

#endif

/* vim: set fdm=marker : */

