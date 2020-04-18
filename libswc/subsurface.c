/* swc: libswc/subsurface.c
 *
 * Copyright (c) 2015-2019 Michael Forney
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

#include "compositor.h"
#include "subsurface.h"
#include "util.h"

#include <stdlib.h>
#include <stdbool.h>
#include <wayland-server.h>

static void
set_position(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y)
{
	struct subsurface *subsurface = wl_resource_get_user_data(resource);

    subsurface->position.x = x;
    subsurface->position.y = y;
    subsurface->position.set = true;

    view_move(subsurface->surface->view, subsurface->parent->view->geometry.x + x, subsurface->parent->view->geometry.y + y);
    view_update(subsurface->surface->view);

	struct compositor_view *comp_view = compositor_view(subsurface->surface->view);
	if (comp_view)
		compositor_view_show(comp_view);
}

static void
place_above(struct wl_client *client, struct wl_resource *resource, struct wl_resource *sibling_resource)
{
    DEBUG("subsurface::place_above\n");
}

static void
place_below(struct wl_client *client, struct wl_resource *resource, struct wl_resource *sibling_resource)
{
    DEBUG("subsurface::place_below\n");
}

static void
set_sync(struct wl_client *client, struct wl_resource *resource)
{
    DEBUG("subsurface::set_sync\n");
}

static void
set_desync(struct wl_client *client, struct wl_resource *resource)
{
    DEBUG("subsurface::set_desync\n");
}

static const struct wl_subsurface_interface subsurface_impl = {
	.destroy = destroy_resource,
	.set_position = set_position,
	.place_above = place_above,
	.place_below = place_below,
	.set_sync = set_sync,
	.set_desync = set_desync,
};

static void
unlink_from_parent(struct subsurface* subsurface)
{
	wl_list_remove(&subsurface->parent_link);
	wl_list_remove(&subsurface->parent_destroy_listener.link);
	subsurface->parent = NULL;
}

static void
subsurface_destroy(struct subsurface *subsurface)
{
	if (subsurface->parent)
	    unlink_from_parent(subsurface);

	struct compositor_view *comp_view = compositor_view(subsurface->surface->view);
	if (comp_view)
	    compositor_view_destroy(comp_view);

    wl_list_remove(&subsurface->surface_destroy_listener.link);

	free(subsurface);
}

static void
subsurface_destroy_handler(struct wl_resource *resource)
{
	struct subsurface *subsurface = wl_resource_get_user_data(resource);
    if (subsurface)
        subsurface_destroy(subsurface);
}

static void
subsurface_handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct subsurface *subsurface;
	subsurface = wl_container_of(listener, subsurface, surface_destroy_listener);

    if (subsurface->resource)
		wl_resource_set_user_data(subsurface->resource, NULL);

	subsurface_destroy(subsurface);
}

static void
subsurface_handle_parent_destroy(struct wl_listener *listener, void *data)
{
	struct subsurface *subsurface;
	subsurface = wl_container_of(listener, subsurface, parent_destroy_listener);

	struct compositor_view *comp_view = compositor_view(subsurface->surface->view);
	if (comp_view)
	    compositor_view_hide(comp_view);

	unlink_from_parent(subsurface);
}

struct subsurface *
subsurface_new(struct wl_client *client, uint32_t version, uint32_t id, struct surface *surface, struct surface *parent)
{
	struct subsurface *subsurface;

	if (!(subsurface = malloc(sizeof(*subsurface))))
		goto error0;

	subsurface->resource = wl_resource_create(client, &wl_subsurface_interface, version, id);
	if (!subsurface->resource)
		goto error1;
	wl_resource_set_implementation(subsurface->resource, &subsurface_impl, subsurface, &subsurface_destroy_handler);

    subsurface->surface = surface;
    subsurface->parent = parent;

	subsurface->surface_destroy_listener.notify = subsurface_handle_surface_destroy;
	subsurface->parent_destroy_listener.notify = subsurface_handle_parent_destroy;
	wl_signal_add(&surface->destroy_signal, &subsurface->surface_destroy_listener);
	wl_signal_add(&parent->destroy_signal, &subsurface->parent_destroy_listener);

	wl_list_insert(&parent->subsurface_list, &subsurface->parent_link);

    subsurface->synchronized = true;

    compositor_create_view(subsurface->surface);

	return subsurface;

error1:
	free(subsurface);
error0:
	return NULL;
}
