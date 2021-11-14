/* swc: libswc/xdg_shell.c
 *
 * Copyright (c) 2014, 2018 Michael Forney
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

#include "xdg_shell.h"
#include "compositor.h"
#include "internal.h"
#include "seat.h"
#include "keyboard.h"
#include "surface.h"
#include "util.h"
#include "window.h"

#include <assert.h>
#include <stdlib.h>
#include <wayland-server.h>
#include "xdg-shell-server-protocol.h"

enum xdg_surface_role {
    XDG_SURFACE_ROLE_NONE,
    XDG_SURFACE_ROLE_TOPLEVEL,
    XDG_SURFACE_ROLE_POPUP,
};

struct xdg_surface {
	struct wl_resource *resource, *role;
	struct surface *surface;
	struct wl_listener surface_destroy_listener, role_destroy_listener;
	uint32_t configure_serial;
    enum xdg_surface_role surface_role;
};

struct xdg_positioner {
	struct wl_resource *resource;

	struct {
        uint32_t width;
        uint32_t height;
	} size;

	struct {
        int32_t x;
        int32_t y;
        uint32_t width;
        uint32_t height;
	} anchor_rect;

	uint32_t anchor;
	uint32_t gravity;
	uint32_t constraint_adjustment;

	struct {
        int32_t x;
        int32_t y;
	} offset;
};

struct xdg_toplevel {
	struct window window;
	struct wl_resource *resource;
	struct wl_array states;
	struct xdg_surface *xdg_surface;
};

struct xdg_popup {
    struct compositor_view* view;
	struct wl_resource *resource;
	struct xdg_surface *xdg_surface;
	struct xdg_surface *parent;
};

/* xdg_positioner */
static struct swc_rectangle
xdg_positioner_get_geometry(struct xdg_positioner *positioner)
{
	struct swc_rectangle geometry = {
		.x = positioner->offset.x,
		.y = positioner->offset.y,
		.width = positioner->size.width,
		.height = positioner->size.height,
	};

	switch (positioner->anchor) {
		case XDG_POSITIONER_ANCHOR_TOP:
		case XDG_POSITIONER_ANCHOR_TOP_LEFT:
		case XDG_POSITIONER_ANCHOR_TOP_RIGHT:
			geometry.y += positioner->anchor_rect.y;
			break;
		case XDG_POSITIONER_ANCHOR_BOTTOM:
		case XDG_POSITIONER_ANCHOR_BOTTOM_LEFT:
		case XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT:
			geometry.y += positioner->anchor_rect.y + positioner->anchor_rect.height;
			break;
		default:
			geometry.y += positioner->anchor_rect.y + positioner->anchor_rect.height / 2;
	}

	switch (positioner->anchor) {
		case XDG_POSITIONER_ANCHOR_LEFT:
		case XDG_POSITIONER_ANCHOR_TOP_LEFT:
		case XDG_POSITIONER_ANCHOR_BOTTOM_LEFT:
			geometry.x += positioner->anchor_rect.x;
			break;
		case XDG_POSITIONER_ANCHOR_RIGHT:
		case XDG_POSITIONER_ANCHOR_TOP_RIGHT:
		case XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT:
			geometry.x += positioner->anchor_rect.x + positioner->anchor_rect.width;
			break;
		default:
			geometry.x += positioner->anchor_rect.x + positioner->anchor_rect.width / 2;
	}

	switch (positioner->gravity) {
		case XDG_POSITIONER_GRAVITY_TOP:
		case XDG_POSITIONER_GRAVITY_TOP_LEFT:
		case XDG_POSITIONER_GRAVITY_TOP_RIGHT:
			geometry.y -= geometry.height;
			break;
		case XDG_POSITIONER_GRAVITY_BOTTOM:
		case XDG_POSITIONER_GRAVITY_BOTTOM_LEFT:
		case XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT:
			geometry.y = geometry.y;
			break;
		default:
			geometry.y -= geometry.height / 2;
	}

	switch (positioner->gravity) {
		case XDG_POSITIONER_GRAVITY_LEFT:
		case XDG_POSITIONER_GRAVITY_TOP_LEFT:
		case XDG_POSITIONER_GRAVITY_BOTTOM_LEFT:
			geometry.x -= geometry.width;
			break;
		case XDG_POSITIONER_GRAVITY_RIGHT:
		case XDG_POSITIONER_GRAVITY_TOP_RIGHT:
		case XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT:
			geometry.x = geometry.x;
			break;
		default:
			geometry.x -= geometry.width / 2;
	}

	return geometry;
}

static void
destroy_positioner(struct wl_resource *resource)
{
	struct xdg_positioner *positioner = wl_resource_get_user_data(resource);

	free(positioner);
}

static void
set_size(struct wl_client *client, struct wl_resource *resource, int32_t width, int32_t height)
{
	struct xdg_positioner *positioner = wl_resource_get_user_data(resource);

	if (width < 1 || height < 1) {
		wl_resource_post_error(resource,
				       XDG_POSITIONER_ERROR_INVALID_INPUT,
				       "width and height must be positives and non-zero");
		return;
	}

	positioner->size.width = width;
	positioner->size.height = height;
}

static void
set_anchor_rect(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct xdg_positioner *positioner = wl_resource_get_user_data(resource);

	if (width < 0 || height < 0) {
		wl_resource_post_error(resource,
				       XDG_POSITIONER_ERROR_INVALID_INPUT,
				       "width and height must be non-negative");
		return;
	}

	positioner->anchor_rect.x = x;
	positioner->anchor_rect.y = y;
	positioner->anchor_rect.width = width;
	positioner->anchor_rect.height = height;
}

static void
set_anchor(struct wl_client *client, struct wl_resource *resource, uint32_t anchor)
{
	struct xdg_positioner *positioner = wl_resource_get_user_data(resource);
	positioner->anchor = anchor;
}

static void
set_gravity(struct wl_client *client, struct wl_resource *resource, uint32_t gravity)
{
	struct xdg_positioner *positioner = wl_resource_get_user_data(resource);
	positioner->gravity = gravity;
}

static void
set_constraint_adjustment(struct wl_client *client, struct wl_resource *resource, uint32_t adjustment)
{
	struct xdg_positioner *positioner = wl_resource_get_user_data(resource);
	positioner->constraint_adjustment = adjustment;
}

static void
set_offset(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y)
{
	struct xdg_positioner *positioner = wl_resource_get_user_data(resource);

	positioner->offset.x = x;
	positioner->offset.y = y;
}

static const struct xdg_positioner_interface positioner_impl = {
	.destroy = destroy_resource,
	.set_size = set_size,
	.set_anchor_rect = set_anchor_rect,
	.set_anchor = set_anchor,
	.set_gravity = set_gravity,
	.set_constraint_adjustment = set_constraint_adjustment,
	.set_offset = set_offset,
};

/* xdg_popup */
static void
destroy_popup(struct wl_resource *resource)
{
	struct xdg_popup *popup = wl_resource_get_user_data(resource);

    compositor_view_destroy(popup->view);
	free(popup);
}

static uint32_t
popup_send_configure(struct xdg_popup *popup, int32_t x, int32_t y, int32_t width, int32_t height) {
	uint32_t serial = wl_display_next_serial(swc.display);

	xdg_popup_send_configure(popup->resource, x, y, width, height);
	xdg_surface_send_configure(popup->xdg_surface->resource, serial);

	return serial;
}

static void
grab(struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial)
{
}

static void
reposition(struct wl_client *client, struct wl_resource *resource, struct wl_resource *positioner, uint32_t token)
{
}

static const struct xdg_popup_interface popup_impl = {
	.destroy = destroy_resource,
	.grab = grab,
	.reposition = reposition,
};

/* xdg_toplevel */
static void
destroy_toplevel(struct wl_resource *resource)
{
	struct xdg_toplevel *toplevel = wl_resource_get_user_data(resource);

	window_finalize(&toplevel->window);
	free(toplevel);
}

static bool
add_state(struct xdg_toplevel *toplevel, uint32_t state)
{
	uint32_t *current_state;

	wl_array_for_each (current_state, &toplevel->states) {
		if (*current_state == state)
			return false;
	}

	if (!(current_state = wl_array_add(&toplevel->states, sizeof(state)))) {
		WARNING("xdg_toplevel: Failed to allocate new state\n");
		return false;
	}

	*current_state = state;
	return true;
}

static bool
remove_state(struct xdg_toplevel *toplevel, uint32_t state)
{
	uint32_t *current_state;

	wl_array_for_each (current_state, &toplevel->states) {
		if (*current_state == state) {
			array_remove(&toplevel->states, current_state, sizeof(state));
			return true;
		}
	}

	return false;
}

static uint32_t
send_configure(struct xdg_toplevel *toplevel, int32_t width, int32_t height) {
	uint32_t serial = wl_display_next_serial(swc.display);

	if (width < 0)
		width = toplevel->window.configure.width;
	if (height < 0)
		height = toplevel->window.configure.height;

	xdg_toplevel_send_configure(toplevel->resource, width, height, &toplevel->states);
	xdg_surface_send_configure(toplevel->xdg_surface->resource, serial);

	return serial;
}

static void
configure(struct window *window, uint32_t width, uint32_t height)
{
	struct xdg_toplevel *toplevel = wl_container_of(window, toplevel, window);

	window->configure.acknowledged = false;
	toplevel->xdg_surface->configure_serial = send_configure(toplevel, width, height);
}

static void
focus(struct window *window)
{
	struct xdg_toplevel *toplevel = wl_container_of(window, toplevel, window);

	add_state(toplevel, XDG_TOPLEVEL_STATE_ACTIVATED);
	send_configure(toplevel, -1, -1);
}

static void
unfocus(struct window *window)
{
	struct xdg_toplevel *toplevel = wl_container_of(window, toplevel, window);

	remove_state(toplevel, XDG_TOPLEVEL_STATE_ACTIVATED);
	send_configure(toplevel, -1, -1);
}

static void
close(struct window *window)
{
	struct xdg_toplevel *toplevel = wl_container_of(window, toplevel, window);

	xdg_toplevel_send_close(toplevel->resource);
}

static void
set_mode(struct window *window, unsigned mode)
{
	struct xdg_toplevel *toplevel = wl_container_of(window, toplevel, window);

	switch (window->mode) {
	case WINDOW_MODE_TILED:
		remove_state(toplevel, XDG_TOPLEVEL_STATE_MAXIMIZED);
		break;
	case WINDOW_MODE_FULLSCREEN:
		remove_state(toplevel, XDG_TOPLEVEL_STATE_FULLSCREEN);
		break;
	}

	switch (mode) {
	case WINDOW_MODE_TILED:
		add_state(toplevel, XDG_TOPLEVEL_STATE_MAXIMIZED);
		break;
	case WINDOW_MODE_FULLSCREEN:
		add_state(toplevel, XDG_TOPLEVEL_STATE_FULLSCREEN);
		break;
	}

	send_configure(toplevel, -1, -1);
}

static const struct window_impl toplevel_window_impl = {
	.configure = configure,
	.focus = focus,
	.unfocus = unfocus,
	.close = close,
	.set_mode = set_mode,
};

static void
set_parent(struct wl_client *client, struct wl_resource *resource, struct wl_resource *parent_resource)
{
	struct xdg_toplevel *toplevel = wl_resource_get_user_data(resource), *parent = NULL;

	if (parent_resource)
		parent = wl_resource_get_user_data(parent_resource);
	window_set_parent(&toplevel->window, parent ? &parent->window : NULL);
}

static void
set_title(struct wl_client *client, struct wl_resource *resource, const char *title)
{
	struct xdg_toplevel *toplevel = wl_resource_get_user_data(resource);
	window_set_title(&toplevel->window, title, -1);
}

static void
set_app_id(struct wl_client *client, struct wl_resource *resource, const char *app_id)
{
	struct xdg_toplevel *toplevel = wl_resource_get_user_data(resource);
	window_set_app_id(&toplevel->window, app_id);
}

static void
show_window_menu(struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial, int32_t x, int32_t y)
{
}

static void
move(struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial)
{
	struct xdg_toplevel *toplevel = wl_resource_get_user_data(resource);
	struct button *button;

	button = pointer_get_button(swc.seat->pointer, serial);
	if (button)
		window_begin_move(&toplevel->window, button);
}

static void
resize(struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial, uint32_t edges)
{
	struct xdg_toplevel *toplevel = wl_resource_get_user_data(resource);
	struct button *button;

	button = pointer_get_button(swc.seat->pointer, serial);
	if (button)
		window_begin_resize(&toplevel->window, edges, button);
}

static void
set_max_size(struct wl_client *client, struct wl_resource *resource, int32_t width, int32_t height)
{
}

static void
set_min_size(struct wl_client *client, struct wl_resource *resource, int32_t width, int32_t height)
{
}

static void
set_maximized(struct wl_client *client, struct wl_resource *resource)
{
}

static void
unset_maximized(struct wl_client *client, struct wl_resource *resource)
{
}

static void
set_fullscreen(struct wl_client *client, struct wl_resource *resource, struct wl_resource *output)
{
}

static void
unset_fullscreen(struct wl_client *client, struct wl_resource *resource)
{
}

static void
set_minimized(struct wl_client *client, struct wl_resource *resource)
{
}

static const struct xdg_toplevel_interface toplevel_impl = {
	.destroy = destroy_resource,
	.set_parent = set_parent,
	.set_title = set_title,
	.set_app_id = set_app_id,
	.show_window_menu = show_window_menu,
	.move = move,
	.resize = resize,
	.set_max_size = set_max_size,
	.set_min_size = set_min_size,
	.set_maximized = set_maximized,
	.unset_maximized = unset_maximized,
	.set_fullscreen = set_fullscreen,
	.unset_fullscreen = unset_fullscreen,
	.set_minimized = set_minimized,
};

static struct xdg_toplevel *
xdg_toplevel_new(struct wl_client *client, uint32_t version, uint32_t id, struct xdg_surface *xdg_surface)
{
	struct xdg_toplevel *toplevel;

	toplevel = malloc(sizeof(*toplevel));
	if (!toplevel)
		goto error0;
	toplevel->xdg_surface = xdg_surface;
	toplevel->resource = wl_resource_create(client, &xdg_toplevel_interface, version, id);
	if (!toplevel->resource)
		goto error1;
	window_initialize(&toplevel->window, &toplevel_window_impl, xdg_surface->surface);
	wl_array_init(&toplevel->states);
	wl_resource_set_implementation(toplevel->resource, &toplevel_impl, toplevel, &destroy_toplevel);
	window_manage(&toplevel->window);

	return toplevel;

error1:
	free(toplevel);
error0:
	return NULL;
}

static struct xdg_popup *
xdg_popup_new(struct wl_client *client, uint32_t version, uint32_t id, struct xdg_surface *xdg_surface, struct xdg_surface *parent)
{
	struct xdg_popup *popup;

	popup = malloc(sizeof(*popup));
	if (!popup)
		goto error0;
	popup->xdg_surface = xdg_surface;
	popup->parent = parent;
	popup->resource = wl_resource_create(client, &xdg_popup_interface, version, id);
	if (!popup->resource)
		goto error1;

    popup->view = compositor_create_view(popup->xdg_surface->surface);
    wl_resource_set_implementation(popup->resource, &popup_impl, popup, &destroy_popup);

	return popup;

error1:
	free(popup);
error0:
	return NULL;
}

/* xdg_surface */
static void
get_toplevel(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
	struct xdg_surface *xdg_surface = wl_resource_get_user_data(resource);
	struct xdg_toplevel *toplevel;

	if (xdg_surface->role) {
		wl_resource_post_error(resource, XDG_WM_BASE_ERROR_ROLE, "surface already has a role");
		return;
	}
	toplevel = xdg_toplevel_new(client, wl_resource_get_version(resource), id, xdg_surface);
	if (!toplevel) {
		wl_client_post_no_memory(client);
		return;
	}
	xdg_surface->role = toplevel->resource;
    xdg_surface->surface_role = XDG_SURFACE_ROLE_TOPLEVEL;
	wl_resource_add_destroy_listener(xdg_surface->role, &xdg_surface->role_destroy_listener);
}

static void
get_popup(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *parent, struct wl_resource *positioner_resource)
{

    struct xdg_surface *popup_surface = wl_resource_get_user_data(resource);
    struct xdg_surface *parent_surface = wl_resource_get_user_data(parent);
    struct xdg_popup *popup;

    popup = xdg_popup_new(client, wl_resource_get_version(resource), id, popup_surface, parent_surface);
    if (!popup) {
        wl_client_post_no_memory(client);
        return;
    }

    popup_surface->role = popup->resource;
    popup_surface->surface_role = XDG_SURFACE_ROLE_POPUP;
	wl_resource_add_destroy_listener(popup_surface->role, &popup_surface->role_destroy_listener);

    struct compositor_view *parent_view = compositor_view(parent_surface->surface->view);
	compositor_view_set_parent(popup->view, parent_view);

    struct xdg_positioner *positioner = wl_resource_get_user_data(positioner_resource);
    struct swc_rectangle geometry = xdg_positioner_get_geometry(positioner);
	view_set_size(&popup->view->base, geometry.width, geometry.height);
    view_move(&popup->view->base, parent_view->base.geometry.x + geometry.x, parent_view->base.geometry.y + geometry.y);

    popup->xdg_surface->configure_serial = popup_send_configure(popup, parent_view->base.geometry.x + geometry.x, parent_view->base.geometry.y + geometry.y, geometry.width, geometry.height);
}

static void
ack_configure(struct wl_client *client, struct wl_resource *resource, uint32_t serial)
{
	struct xdg_surface *xdg_surface = wl_resource_get_user_data(resource);

    switch (xdg_surface->surface_role) {
        case XDG_SURFACE_ROLE_NONE:
            return;
        case XDG_SURFACE_ROLE_POPUP:
            return;
    }

	struct window *window;
	window = wl_resource_get_user_data(xdg_surface->role);
	if (window && serial == xdg_surface->configure_serial)
	 	window->configure.acknowledged = true;
}

static void
set_window_geometry(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
}

static const struct xdg_surface_interface xdg_surface_impl = {
	.destroy = destroy_resource,
	.get_toplevel = get_toplevel,
	.get_popup = get_popup,
	.ack_configure = ack_configure,
	.set_window_geometry = set_window_geometry,
};

static void
handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct xdg_surface *xdg_surface = wl_container_of(listener, xdg_surface, surface_destroy_listener);

	wl_resource_destroy(xdg_surface->resource);
}

static void
handle_role_destroy(struct wl_listener *listener, void *data)
{
	struct xdg_surface *xdg_surface = wl_container_of(listener, xdg_surface, role_destroy_listener);

	xdg_surface->role = NULL;
}

static void
destroy_xdg_surface(struct wl_resource *resource)
{
	struct xdg_surface *xdg_surface = wl_resource_get_user_data(resource);

	wl_list_remove(&xdg_surface->surface_destroy_listener.link);
	if (xdg_surface->role)
		wl_resource_destroy(xdg_surface->role);
	free(xdg_surface);
}

static struct xdg_surface *
xdg_surface_new(struct wl_client *client, uint32_t version, uint32_t id, struct surface *surface)
{
	struct xdg_surface *xdg_surface;

	xdg_surface = malloc(sizeof(*xdg_surface));
	if (!xdg_surface)
		goto error0;
	xdg_surface->resource = wl_resource_create(client, &xdg_surface_interface, version, id);
	if (!xdg_surface->resource)
		goto error1;
	xdg_surface->surface = surface;
	xdg_surface->surface_destroy_listener.notify = &handle_surface_destroy;
	xdg_surface->role = NULL;
	xdg_surface->role_destroy_listener.notify = &handle_role_destroy;
	wl_resource_add_destroy_listener(surface->resource, &xdg_surface->surface_destroy_listener);
	wl_resource_set_implementation(xdg_surface->resource, &xdg_surface_impl, xdg_surface, destroy_xdg_surface);

	return xdg_surface;

error1:
	free(xdg_surface);
error0:
	return NULL;
}

/* xdg_shell */
static void
create_positioner(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
	struct xdg_positioner *positioner;
	uint32_t version;

	positioner = malloc(sizeof(*positioner));
	if (!positioner)
		goto error0;
	version = wl_resource_get_version(resource);
	positioner->resource = wl_resource_create(client, &xdg_positioner_interface, version, id);
	if (!positioner->resource)
		goto error1;
	wl_resource_set_implementation(positioner->resource, &positioner_impl, positioner, &destroy_positioner);
	return;

error1:
	free(positioner);
error0:
	wl_resource_post_no_memory(resource);
}

static void
get_xdg_surface(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface_resource)
{
	struct xdg_surface *xdg_surface;
	struct surface *surface = wl_resource_get_user_data(surface_resource);

	xdg_surface = xdg_surface_new(client, wl_resource_get_version(resource), id, surface);
	if (!xdg_surface)
		wl_client_post_no_memory(client);
}

static void
pong(struct wl_client *client, struct wl_resource *resource, uint32_t serial)
{
}

static const struct xdg_wm_base_interface wm_base_impl = {
	.destroy = destroy_resource,
	.create_positioner = create_positioner,
	.get_xdg_surface = get_xdg_surface,
	.pong = pong,
};

static void
bind_wm_base(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource;

	resource = wl_resource_create(client, &xdg_wm_base_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &wm_base_impl, NULL, NULL);
}

struct wl_global *
xdg_shell_create(struct wl_display *display)
{
	return wl_global_create(display, &xdg_wm_base_interface, 1, NULL, &bind_wm_base);
}
