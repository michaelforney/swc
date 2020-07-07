/* swc: libswc/compositor.h
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

#ifndef SWC_COMPOSITOR_H
#define SWC_COMPOSITOR_H

#include "view.h"

#include <stdbool.h>
#include <pixman.h>
#include <wayland-server.h>

struct swc_compositor {
	struct pointer_handler *const pointer_handler;
	struct {
		/**
		 * Emitted when a new surface is created.
		 *
		 * The data argument of the signal refers to the surface that has been
		 * created.
		 */
		struct wl_signal new_surface;
	} signal;
};

bool compositor_initialize(void);
void compositor_finalize(void);

struct compositor_view {
	struct view base;
	struct surface *surface;
	struct wld_buffer *buffer;
	struct window *window;
	struct compositor_view *parent;

	/* Whether or not the view is visible (mapped). */
	bool visible;

	/* The box that the surface covers (including it's border). */
	pixman_box32_t extents;

	/* The region that is covered by opaque regions of surfaces above this
	 * surface. */
	pixman_region32_t clip;

	struct {
		uint32_t width;
		uint32_t color;
		bool damaged;
	} border;

	struct wl_list link;
	struct wl_signal destroy_signal;
};

struct compositor_view *compositor_create_view(struct surface *surface);

void compositor_view_destroy(struct compositor_view *view);

/**
 * Returns view as a compositor_view, or NULL if view is not a compositor_view.
 */
struct compositor_view *compositor_view(struct view *view);

void compositor_view_set_parent(struct compositor_view *view, struct compositor_view *parent);

void compositor_view_show(struct compositor_view *view);
void compositor_view_hide(struct compositor_view *view);

void compositor_view_set_border_color(struct compositor_view *view, uint32_t color);
void compositor_view_set_border_width(struct compositor_view *view, uint32_t width);

#endif
