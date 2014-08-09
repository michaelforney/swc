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
    SWC_WINDOW_RESIZED,

    /**
     * Sent when the window's parent changes.
     *
     * This can occur when the window becomes a transient for another window,
     * or becomes a toplevel window.
     */
    SWC_WINDOW_PARENT_CHANGED
};

struct swc_window
{
    struct wl_signal event_signal;

    char * title;
    char * class;

    enum
    {
        SWC_WINDOW_STATE_NONE,
        SWC_WINDOW_STATE_NORMAL
    } state;

    struct swc_window * parent;
};

/**
 * Request that the specified window close.
 */
void swc_window_close(struct swc_window * window);

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
 * Set the window's position.
 *
 * The x and y coordinates refer to the top-left corner of the actual contents
 * of the window and should be adjusted for the border size.
 */
void swc_window_set_position(struct swc_window * window, int32_t x, int32_t y);

/**
 * Set the window's size.
 *
 * The width and height refer to the dimension of the actual contents of the
 * window and should be adjusted for the border size.
 */
void swc_window_set_size(struct swc_window * window,
                         uint32_t width, uint32_t height);

/**
 * Set the window's size and position.
 *
 * This is a convenience function that is equivalent to calling
 * swc_window_set_size and then swc_window_set_position.
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

/**
 * Begin an interactive move of the specified window.
 */
void swc_window_begin_move(struct swc_window * window);

/**
 * End an interactive move of the specified window.
 */
void swc_window_end_move(struct swc_window * window);

enum
{
    SWC_WINDOW_EDGE_AUTO    = 0,
    SWC_WINDOW_EDGE_TOP     = (1 << 0),
    SWC_WINDOW_EDGE_BOTTOM  = (1 << 1),
    SWC_WINDOW_EDGE_LEFT    = (1 << 2),
    SWC_WINDOW_EDGE_RIGHT   = (1 << 3)
};

/**
 * Begin an interactive resize of the specified window.
 */
void swc_window_begin_resize(struct swc_window * window, uint32_t edges);

/**
 * End an interactive resize of the specified window.
 */
void swc_window_end_resize(struct swc_window * window);

/* }}} */

/* Bindings {{{ */

enum
{
    SWC_MOD_CTRL    = 1 << 0,
    SWC_MOD_ALT     = 1 << 1,
    SWC_MOD_LOGO    = 1 << 2,
    SWC_MOD_SHIFT   = 1 << 3,
    SWC_MOD_ANY     = ~0
};

enum swc_binding_type
{
    SWC_BINDING_KEY,
    SWC_BINDING_BUTTON,
};

typedef void (* swc_binding_handler_t)(void * data, uint32_t time,
                                       uint32_t value, uint32_t state);

/**
 * Register a new input binding.
 *
 * Returns 0 on success, negative error code otherwise.
 */
int swc_add_binding(enum swc_binding_type type,
                    uint32_t modifiers, uint32_t value,
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
     * Called when a new screen is created.
     */
    void (* new_screen)(struct swc_screen * screen);

    /**
     * Called when a new window is created.
     *
     * The window begins in the WITHDRAWN state and should not be managed until
     * it changes to the TOPLEVEL state.
     */
    void (* new_window)(struct swc_window * window);
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

