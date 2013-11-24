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

/* Rectangles {{{ */
struct swc_rectangle
{
    int32_t x, y;
    uint32_t width, height;
};
/* }}} */

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

void swc_window_show(struct swc_window * window);
void swc_window_hide(struct swc_window * window);
void swc_window_focus(struct swc_window * window);
void swc_window_set_geometry(struct swc_window * window,
                             const struct swc_rectangle * geometry);
void swc_window_set_border(struct swc_window * window,
                           uint32_t color, uint32_t width);
/* }}} */

/* Screens {{{ */
enum
{
    SWC_SCREEN_GEOMETRY_CHANGED
};

struct swc_screen
{
    struct wl_signal event_signal;

    /* The total area of the screen */
    struct swc_rectangle total_geometry;

    /* The area of the screen available for placing windows */
    struct swc_rectangle usable_geometry;
};
/* }}} */

/* Bindings {{{ */
#define SWC_MOD_CTRL    (1 << 0)
#define SWC_MOD_ALT     (1 << 1)
#define SWC_MOD_LOGO    (1 << 2)
#define SWC_MOD_SHIFT   (1 << 3)
#define SWC_MOD_ANY     (-1)

typedef void (* swc_binding_handler_t)(uint32_t time, uint32_t value,
                                       void * data);

struct swc_binding
{
    uint32_t value;
    uint32_t modifiers;
    swc_binding_handler_t handler;
    void * data;
};

void swc_add_key_binding(uint32_t modifiers, uint32_t value,
                         swc_binding_handler_t handler, void * data);
/* }}} */

/* Events {{{ */
struct swc_event
{
    uint32_t type;
    void * data;
};
/* }}} */

struct swc_manager
{
    void (* new_window)(struct swc_window * window);
    void (* new_screen)(struct swc_screen * screen);
};

bool swc_initialize(struct wl_display * display,
                    struct wl_event_loop * event_loop,
                    const struct swc_manager * manager);
void swc_finalize();

#endif

/* vim: set fdm=marker : */

