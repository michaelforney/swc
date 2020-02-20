/* swc: libswc/seat.h
 *
 * Copyright (c) 2013-2019 Michael Forney
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

#ifndef SWC_SEAT_H
#define SWC_SEAT_H

#include <wayland-server.h>

struct wl_display;

struct swc_seat {
	struct pointer *pointer;
	struct keyboard *keyboard;
	struct data_device *data_device;
	struct wl_listener keyboard_focus_listener;
	struct wl_listener data_device_listener;
};

struct swc_seat *seat_create(struct wl_display *display, const char *name);
void seat_destroy(struct swc_seat *seat);
void seat_handle_keyboard_focus_event(struct wl_listener *listener, void *data);
void seat_handle_data_device_event(struct wl_listener *listener, void *data);

struct wl_seat_interface seat_impl;

#endif
