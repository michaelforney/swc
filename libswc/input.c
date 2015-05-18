/* swc: input.c
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

#include "input.h"
#include "compositor.h"
#include "event.h"
#include "surface.h"
#include "util.h"

static inline void
focus(struct input_focus *input_focus,
      struct compositor_view *view)
{
	struct wl_resource *resource = NULL;

	if (view) {
		struct wl_client *client;

		client = wl_resource_get_client(view->surface->resource);
		resource = wl_resource_find_for_client(&input_focus->resources, client);

		wl_signal_add(&view->destroy_signal,
		              &input_focus->view_destroy_listener);

		if (resource)
			input_focus->handler->enter(input_focus->handler, resource, view);
	}

	input_focus->view = view;
	input_focus->resource = resource;
}

static inline void
unfocus(struct input_focus *input_focus)
{
	if (input_focus->view)
		wl_list_remove(&input_focus->view_destroy_listener.link);

	if (input_focus->resource) {
		input_focus->handler->leave(input_focus->handler, input_focus->resource,
		                            input_focus->view);
	}
}

static void
handle_focus_view_destroy(struct wl_listener *listener,
                          void *data)
{
	struct input_focus *input_focus = wl_container_of(listener, input_focus, view_destroy_listener);

	input_focus->resource = NULL;
	input_focus->view = NULL;
}

bool
input_focus_initialize(struct input_focus *input_focus,
                       struct input_focus_handler *handler)
{
	input_focus->resource = NULL;
	input_focus->view = NULL;
	input_focus->view_destroy_listener.notify = &handle_focus_view_destroy;
	input_focus->handler = handler;

	wl_list_init(&input_focus->resources);
	wl_signal_init(&input_focus->event_signal);

	return true;
}

void
input_focus_finalize(struct input_focus *input_focus)
{
	/* XXX: Destroy resources? */
}

void
input_focus_add_resource(struct input_focus *input_focus,
                         struct wl_resource *resource)
{
	/* If this new input resource corresponds to our focus, set it as our
     * focus. */
	if (input_focus->view) {
		struct wl_client *client, *surface_client;

		client = wl_resource_get_client(resource);
		surface_client = wl_resource_get_client(input_focus->view->surface->resource);

		if (client == surface_client) {
			input_focus->handler->enter(input_focus->handler, resource,
			                            input_focus->view);
			input_focus->resource = resource;
		}
	}

	wl_list_insert(&input_focus->resources, wl_resource_get_link(resource));
}

void
input_focus_remove_resource(struct input_focus *input_focus,
                            struct wl_resource *resource)
{
	if (resource == input_focus->resource)
		input_focus->resource = NULL;

	remove_resource(resource);
}

void
input_focus_set(struct input_focus *input_focus,
                struct compositor_view *view)
{
	struct input_focus_event_data data;

	if (view == input_focus->view)
		return;

	data.old = input_focus->view;
	data.new = view;

	/* Unfocus previously focused view. */
	unfocus(input_focus);

	/* Focus new view, if given. */
	focus(input_focus, view);

	send_event(&input_focus->event_signal, INPUT_FOCUS_EVENT_CHANGED, &data);

	return;
}
