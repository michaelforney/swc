/* swc: libswc/screencopy.h
 *
 * Copyright (c) 2026 sewn <sewn@disroot.org>
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
 
#ifndef SWC_SCREENCOPY_H
#define SWC_SCREENCOPY_H

#include "swc.h"

#include <pixman.h>
#include <stdbool.h>
#include <stdint.h>
#include <wayland-server.h>

struct screencopy_frame {
	struct wl_resource *resource;
	struct wl_resource *client_buffer;
	struct screen *screen;

	bool with_damage;
	bool overlay_cursor;

	struct wl_listener screen_attach;

	struct swc_rectangle geom;
	uint32_t stride;
};

struct wl_global *
screencopy_manager_create(struct wl_display *display);

#endif
