/* swc: libswc/seat.c
 *
 * Copyright (c) 2013-2020 Michael Forney
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
#include "seat.h"
#include "compositor.h"
#include "data_device.h"
#include "event.h"
#include "keyboard.h"
#include "pointer.h"
#include "surface.h"
#include <stdint.h>

void
seat_handle_keyboard_focus_event(struct wl_listener *listener, void *data)
{
	struct swc_seat *seat = wl_container_of(listener, seat, keyboard_focus_listener);
	struct event *ev = data;
	struct input_focus_event_data *event_data = ev->data;

	if (ev->type != INPUT_FOCUS_EVENT_CHANGED)
		return;

	if (event_data->new) {
		struct wl_client *client = wl_resource_get_client(event_data->new->surface->resource);

		/* Offer the selection to the new focus. */
		data_device_offer_selection(seat->data_device, client);
	}
}

void
seat_handle_data_device_event(struct wl_listener *listener, void *data)
{
	struct swc_seat *seat = wl_container_of(listener, seat, data_device_listener);
	struct event *ev = data;

	if (ev->type != DATA_DEVICE_EVENT_SELECTION_CHANGED)
		return;

	if (seat->keyboard->focus.client)
		data_device_offer_selection(seat->data_device, seat->keyboard->focus.client);
}

/* Wayland Seat Interface */

static void
get_pointer(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
	struct swc_seat *seat = wl_resource_get_user_data(resource);

	if (!pointer_bind(seat->pointer, client, wl_resource_get_version(resource), id))
		wl_resource_post_no_memory(resource);
}

static void
get_keyboard(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
	struct swc_seat *seat = wl_resource_get_user_data(resource);

	if (!keyboard_bind(seat->keyboard, client, wl_resource_get_version(resource), id))
		wl_resource_post_no_memory(resource);
}

static void
get_touch(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
	/* XXX: Implement */
}

struct wl_seat_interface seat_impl = {
	.get_pointer = get_pointer,
	.get_keyboard = get_keyboard,
	.get_touch = get_touch,
};

