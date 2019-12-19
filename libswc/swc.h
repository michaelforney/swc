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

#ifdef __cplusplus
extern "C" {
#endif

struct libinput_device;
struct wl_display;
struct wl_event_loop;

/* Rectangles {{{ */

struct swc_rectangle {
	int32_t x, y;
	uint32_t width, height;
};

/* }}} */

/* Screens {{{ */

struct swc_screen_handler {
	/**
	 * Called when the screen is about to be destroyed.
	 *
	 * After this is called, the screen is no longer valid.
	 */
	void (*destroy)(void *data);

	/**
	 * Called when the total area of the screen has changed.
	 */
	void (*geometry_changed)(void *data);

	/**
	 * Called when the geometry of the screen available for laying out windows has
	 * changed.
	 *
	 * A window manager should respond by making sure all visible windows are
	 * within this area.
	 */
	void (*usable_geometry_changed)(void *data);

	/**
	 * Called when the pointer enters the screen.
	 */
	void (*entered)(void *data);
};

struct swc_screen {
	/**
	 * The total area of the screen.
	 */
	struct swc_rectangle geometry;

	/**
	 * The area of the screen available for placing windows.
	 */
	struct swc_rectangle usable_geometry;
};

/**
 * Set the handler associated with this screen.
 */
void swc_screen_set_handler(struct swc_screen *screen, const struct swc_screen_handler *handler, void *data);

/* }}} */

/* Windows {{{ */

struct swc_window_handler {
	/**
	 * Called when the window is about to be destroyed.
	 *
	 * After this is called, the window is no longer valid.
	 */
	void (*destroy)(void *data);

	/**
	 * Called when the window's title changes.
	 */
	void (*title_changed)(void *data);

	/**
	 * Called when the window's application identifier changes.
	 */
	void (*app_id_changed)(void *data);

	/**
	 * Called when the window's parent changes.
	 *
	 * This can occur when the window becomes a transient for another window, or
	 * becomes a toplevel window.
	 */
	void (*parent_changed)(void *data);

	/**
	 * Called when the pointer enters the window.
	 */
	void (*entered)(void *data);

	/**
	 * Called when the window wants to initiate an interactive move, but the
	 * window is not in stacked mode.
	 *
	 * The window manager may respond by changing the window's mode, after which
	 * the interactive move will be honored.
	 */
	void (*move)(void *data);

	/**
	 * Called when the window wants to initiate an interactive resize, but the
	 * window is not in stacked mode.
	 *
	 * The window manager may respond by changing the window's mode, after which
	 * the interactive resize will be honored.
	 */
	void (*resize)(void *data);
};

struct swc_window {
	char *title;
	char *app_id;

	struct swc_window *parent;
};

/**
 * Set the handler associated with this window.
 */
void swc_window_set_handler(struct swc_window *window, const struct swc_window_handler *handler, void *data);

/**
 * Request that the specified window close.
 */
void swc_window_close(struct swc_window *window);

/**
 * Make the specified window visible.
 */
void swc_window_show(struct swc_window *window);

/**
 * Make the specified window hidden.
 */
void swc_window_hide(struct swc_window *window);

/**
 * Set the keyboard focus to the specified window.
 *
 * If window is NULL, the keyboard will have no focus.
 */
void swc_window_focus(struct swc_window *window);

/**
 * Sets the window to stacked mode.
 *
 * A window in this mode has its size specified by the client. The window's
 * viewport will be adjusted to the size of the buffer attached by the
 * client.
 *
 * Use of this mode is required to allow interactive moving and resizing.
 */
void swc_window_set_stacked(struct swc_window *window);

/**
 * Sets the window to tiled mode.
 *
 * A window in this mode has its size specified by the window manager.
 * Additionally, swc will configure the window to operate in a tiled or
 * maximized state in order to prevent the window from drawing shadows.
 *
 * It is invalid to interactively move or resize a window in tiled mode.
 */
void swc_window_set_tiled(struct swc_window *window);

/**
 * Sets the window to fullscreen mode.
 */
void swc_window_set_fullscreen(struct swc_window *window, struct swc_screen *screen);

/**
 * Set the window's position.
 *
 * The x and y coordinates refer to the top-left corner of the actual contents
 * of the window and should be adjusted for the border size.
 */
void swc_window_set_position(struct swc_window *window, int32_t x, int32_t y);

/**
 * Set the window's size.
 *
 * The width and height refer to the dimension of the actual contents of the
 * window and should be adjusted for the border size.
 */
void swc_window_set_size(struct swc_window *window, uint32_t width, uint32_t height);

/**
 * Set the window's size and position.
 *
 * This is a convenience function that is equivalent to calling
 * swc_window_set_size and then swc_window_set_position.
 */
void swc_window_set_geometry(struct swc_window *window, const struct swc_rectangle *geometry);

/**
 * Set the window's border color and width.
 *
 * NOTE: The window's geometry remains unchanged, and should be updated if a
 *       fixed top-left corner of the border is desired.
 */
void swc_window_set_border(struct swc_window *window, uint32_t color, uint32_t width);

/**
 * Begin an interactive move of the specified window.
 */
void swc_window_begin_move(struct swc_window *window);

/**
 * End an interactive move of the specified window.
 */
void swc_window_end_move(struct swc_window *window);

enum {
	SWC_WINDOW_EDGE_AUTO = 0,
	SWC_WINDOW_EDGE_TOP = (1 << 0),
	SWC_WINDOW_EDGE_BOTTOM = (1 << 1),
	SWC_WINDOW_EDGE_LEFT = (1 << 2),
	SWC_WINDOW_EDGE_RIGHT = (1 << 3)
};

/**
 * Begin an interactive resize of the specified window.
 */
void swc_window_begin_resize(struct swc_window *window, uint32_t edges);

/**
 * End an interactive resize of the specified window.
 */
void swc_window_end_resize(struct swc_window *window);

/* }}} */

/* Bindings {{{ */

enum {
	SWC_MOD_CTRL = 1 << 0,
	SWC_MOD_ALT = 1 << 1,
	SWC_MOD_LOGO = 1 << 2,
	SWC_MOD_SHIFT = 1 << 3,
	SWC_MOD_ANY = ~0
};

enum swc_binding_type {
	SWC_BINDING_KEY,
	SWC_BINDING_BUTTON,
};

typedef void (*swc_binding_handler)(void *data, uint32_t time, uint32_t value, uint32_t state);

/**
 * Register a new input binding.
 *
 * Returns 0 on success, negative error code otherwise.
 */
int swc_add_binding(enum swc_binding_type type, uint32_t modifiers, uint32_t value, swc_binding_handler handler, void *data);

/* }}} */

/**
 * This is a user-provided structure that swc will use to notify the display
 * server of new windows, screens and input devices.
 */
struct swc_manager {
	/**
	 * Called when a new screen is created.
	 */
	void (*new_screen)(struct swc_screen *screen);

	/**
	 * Called when a new window is created.
	 */
	void (*new_window)(struct swc_window *window);

	/**
	 * Called when a new input device is detected.
	 */
	void (*new_device)(struct libinput_device *device);

	/**
	 * Called when the session gets activated (for example, startup or VT switch).
	 */
	void (*activate)(void);

	/**
	 * Called when the session gets deactivated.
	 */
	void (*deactivate)(void);
};

/**
 * Initializes the compositor using the specified display, event_loop, and
 * manager.
 */
bool swc_initialize(struct wl_display *display, struct wl_event_loop *event_loop, const struct swc_manager *manager);

/**
 * Stops the compositor, releasing any used resources.
 */
void swc_finalize(void);

#ifdef __cplusplus
}
#endif

#endif

/* vim: set fdm=marker : */
