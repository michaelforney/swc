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
    /**
     * Sent when the window is about to be destroyed.
     *
     * After this event is sent, the window is not longer valid.
     */
    SWC_WINDOW_DESTROYED,

    /**
     * Sent when the window's title changes.
     */
    SWC_WINDOW_TITLE_CHANGED,

    /**
     * Sent when the window's class changes.
     */
    SWC_WINDOW_CLASS_CHANGED,

    /**
     * Sent when the window's state changes.
     *
     * The display server should adjust the window's placement and visibility
     * accordingly.
     */
    SWC_WINDOW_STATE_CHANGED,

    /**
     * Sent when the pointer enters the window.
     */
    SWC_WINDOW_ENTERED,

    /**
     * Sent when the window's size has changed.
     */
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

/**
 * Make the specified window visible.
 */
void swc_window_show(struct swc_window * window);

/**
 * Make the specified window hidden.
 */
void swc_window_hide(struct swc_window * window);

/**
 * Set the keyboard focus to the specified window.
 *
 * If window is NULL, the keyboard will have no focus.
 */
void swc_window_focus(struct swc_window * window);

/**
 * Set the window's geometry.
 *
 * The geometry's coordinates refer to the actual contents of the window, and
 * should be adjusted for the border size.
 */
void swc_window_set_geometry(struct swc_window * window,
                             const struct swc_rectangle * geometry);

/**
 * Set the window's border color and width.
 *
 * NOTE: The window's geometry remains unchanged, and should be updated if a
 *       fixed top-left corner of the border is desired.
 */
void swc_window_set_border(struct swc_window * window,
                           uint32_t color, uint32_t width);

/* }}} */

/* Screens {{{ */

enum
{
    /**
     * Sent when the screen is about to be destroyed.
     *
     * After this event is sent, the screen is not longer valid.
     */
    SWC_SCREEN_DESTROYED,

    /**
     * Sent when the total area of the screen is changed.
     */
    SWC_SCREEN_GEOMETRY_CHANGED,

    /**
     * Sent when the geometry of the screen available for laying out windows is
     * changed.
     *
     * Display servers should respond to this event by making sure all visible
     * windows are within this area.
     */
    SWC_SCREEN_USABLE_GEOMETRY_CHANGED
};

struct swc_screen
{
    struct wl_signal event_signal;

    /**
     * The total area of the screen.
     */
    struct swc_rectangle geometry;

    /**
     * The area of the screen available for placing windows.
     */
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

void swc_add_key_binding(uint32_t modifiers, uint32_t value,
                         swc_binding_handler_t handler, void * data);

/* }}} */

/* Events {{{ */

/**
 * An event is the data passed to the listeners of the event_signals of various
 * objects.
 */
struct swc_event
{
    /**
     * The type of event that was sent.
     *
     * The meaning of this field depends on the type of object containing the
     * event_signal that passed this event.
     */
    uint32_t type;

    /**
     * Data specific to the event type.
     *
     * Unless explicitly stated in the description of the event type, this
     * value is undefined.
     */
    void * data;
};

/* }}} */

/**
 * This is a user-provided structure that swc will use to notify the display
 * server of new windows and screens.
 */
struct swc_manager
{
    /**
     * Called when a new window is created.
     *
     * The window begins in the WITHDRAWN state and should not be managed until
     * it changes to the TOPLEVEL state.
     */
    void (* new_window)(struct swc_window * window);

    /**
     * Called when a new screen is created.
     */
    void (* new_screen)(struct swc_screen * screen);
};

/**
 * Initializes the compositor using the specified display, event_loop, and
 * manager.
 */
bool swc_initialize(struct wl_display * display,
                    struct wl_event_loop * event_loop,
                    const struct swc_manager * manager);

/**
 * Stops the compositor, releasing any used resources.
 */
void swc_finalize();

#endif

/* vim: set fdm=marker : */

