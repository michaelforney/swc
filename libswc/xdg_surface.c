/* swc: libswc/xdg_surface.c
 *
 * Copyright (c) 2014 Michael Forney
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

#include "xdg_surface.h"
#include "compositor.h"
#include "internal.h"
#include "seat.h"
#include "surface.h"
#include "util.h"
#include "view.h"
#include "window.h"
#include "protocol/xdg-shell-server-protocol.h"

#include <stdlib.h>

struct xdg_surface
{
    struct window window;
    struct wl_resource * resource;
    struct wl_listener surface_destroy_listener;
    struct wl_array states;
    uint32_t configure_serial;
};

static bool add_state(struct xdg_surface * xdg_surface, uint32_t state)
{
    uint32_t * current_state;

    wl_array_for_each(current_state, &xdg_surface->states)
    {
        if (*current_state == state)
            return false;
    }

    if (!(current_state = wl_array_add(&xdg_surface->states, sizeof state)))
    {
        WARNING("xdg_surface: Failed to allocate new state\n");
        return false;
    }

    *current_state = state;
    return true;
}

static bool remove_state(struct xdg_surface * xdg_surface, uint32_t state)
{
    uint32_t * current_state;

    wl_array_for_each(current_state, &xdg_surface->states)
    {
        if (*current_state == state)
        {
            swc_array_remove(&xdg_surface->states, current_state, sizeof state);
            return true;
        }
    }

    return false;
}

static void configure(struct window * window, uint32_t width, uint32_t height)
{
    struct xdg_surface * xdg_surface
        = wl_container_of(window, xdg_surface, window);

    window->configure.acknowledged = false;
    xdg_surface->configure_serial = wl_display_next_serial(swc.display);
    xdg_surface_send_configure(xdg_surface->resource, width, height,
                               &xdg_surface->states,
                               xdg_surface->configure_serial);
}

static void focus(struct window * window)
{
    struct xdg_surface * xdg_surface
        = wl_container_of(window, xdg_surface, window);

    add_state(xdg_surface, XDG_SURFACE_STATE_ACTIVATED);
    configure(window, window->configure.width, window->configure.height);
}

static void unfocus(struct window * window)
{
    struct xdg_surface * xdg_surface
        = wl_container_of(window, xdg_surface, window);

    remove_state(xdg_surface, XDG_SURFACE_STATE_ACTIVATED);
    configure(window, window->configure.width, window->configure.height);
}

static void close(struct window * window)
{
    struct xdg_surface * xdg_surface
        = wl_container_of(window, xdg_surface, window);

    xdg_surface_send_close(xdg_surface->resource);
}

static void set_mode(struct window * window, unsigned mode)
{
    struct xdg_surface * xdg_surface
        = wl_container_of(window, xdg_surface, window);

    switch (window->mode)
    {
        case WINDOW_MODE_TILED:
            remove_state(xdg_surface, XDG_SURFACE_STATE_MAXIMIZED);
            break;
        case WINDOW_MODE_FULLSCREEN:
            remove_state(xdg_surface, XDG_SURFACE_STATE_FULLSCREEN);
            break;
    }

    switch (mode)
    {
        case WINDOW_MODE_TILED:
            add_state(xdg_surface, XDG_SURFACE_STATE_MAXIMIZED);
            break;
        case WINDOW_MODE_FULLSCREEN:
            add_state(xdg_surface, XDG_SURFACE_STATE_FULLSCREEN);
            break;
    }
}

static const struct window_impl xdg_surface_window_impl = {
    .configure = &configure,
    .focus = &focus,
    .unfocus = &unfocus,
    .close = &close,
    .set_mode = &set_mode,
};

static void destroy(struct wl_client * client, struct wl_resource * resource)
{
    wl_resource_destroy(resource);
}

static void set_parent(struct wl_client * client, struct wl_resource * resource,
                       struct wl_resource * parent_resource)
{
    struct xdg_surface * xdg_surface = wl_resource_get_user_data(resource),
                       * parent_surface;
    struct window * parent_window = NULL;

    if (parent_resource)
    {
        parent_surface = wl_resource_get_user_data(parent_resource);
        parent_window = &parent_surface->window;
    }

    window_set_parent(&xdg_surface->window, parent_window);
}

static void set_title(struct wl_client * client, struct wl_resource * resource,
                      const char * title)
{
    struct xdg_surface * xdg_surface = wl_resource_get_user_data(resource);

    window_set_title(&xdg_surface->window, title, -1);
}

static void set_app_id(struct wl_client * client, struct wl_resource * resource,
                       const char * app_id)
{
    struct xdg_surface * surface = wl_resource_get_user_data(resource);

    window_set_app_id(&surface->window, app_id);
}

static void show_window_menu(struct wl_client * client,
                             struct wl_resource * resource,
                             struct wl_resource * seat_resource,
                             uint32_t serial, int32_t x, int32_t y)
{
}

static void move(struct wl_client * client, struct wl_resource * resource,
                 struct wl_resource * seat_resource, uint32_t serial)
{
    struct xdg_surface * xdg_surface = wl_resource_get_user_data(resource);
    struct button * button;

    if (!(button = pointer_get_button(swc.seat->pointer, serial)))
        return;

    window_begin_move(&xdg_surface->window, button);
}

static void resize(struct wl_client * client, struct wl_resource * resource,
                   struct wl_resource * seat_resource, uint32_t serial,
                   uint32_t edges)
{
    struct xdg_surface * xdg_surface = wl_resource_get_user_data(resource);
    struct button * button;

    if (!(button = pointer_get_button(swc.seat->pointer, serial)))
        return;

    window_begin_resize(&xdg_surface->window, edges, button);
}

static void ack_configure(struct wl_client * client,
                          struct wl_resource * resource, uint32_t serial)
{
    struct xdg_surface * xdg_surface = wl_resource_get_user_data(resource);

    if (serial == xdg_surface->configure_serial)
        xdg_surface->window.configure.acknowledged = true;
}

static void set_window_geometry(struct wl_client * client,
                                struct wl_resource * resource,
                                int32_t x, int32_t y,
                                int32_t width, int32_t height)
{
    /* TODO: Implement set_window_geometry. */
}

static void set_maximized(struct wl_client * client,
                          struct wl_resource * resource)
{
    /* TODO: Implement set_maximized. */
}

static void unset_maximized(struct wl_client * client,
                            struct wl_resource * resource)
{
    /* TODO: Implement unset_maximized. */
}

static void set_fullscreen(struct wl_client * client,
                           struct wl_resource * resource,
                           struct wl_resource * output_resource)
{
    /* TODO: Implement set_fullscreen. */
}

static void unset_fullscreen(struct wl_client * client,
                             struct wl_resource * resource)
{
    /* TODO: Implement unset_fullscreen. */
}

static void set_minimized(struct wl_client * client,
                          struct wl_resource * resource)
{
    /* TODO: Implement set_minimized. */
}

static const struct xdg_surface_interface xdg_surface_implementation = {
    .destroy = &destroy,
    .set_parent = &set_parent,
    .set_title = &set_title,
    .set_app_id = &set_app_id,
    .show_window_menu = &show_window_menu,
    .move = &move,
    .resize = &resize,
    .ack_configure = &ack_configure,
    .set_window_geometry = &set_window_geometry,
    .set_maximized = &set_maximized,
    .unset_maximized = &unset_maximized,
    .set_fullscreen = &set_fullscreen,
    .unset_fullscreen = &unset_fullscreen,
    .set_minimized = &set_minimized,
};

static void handle_surface_destroy(struct wl_listener * listener, void * data)
{
    struct xdg_surface * xdg_surface
        = wl_container_of (listener, xdg_surface, surface_destroy_listener);

    wl_resource_destroy(xdg_surface->resource);
}

static void destroy_xdg_surface(struct wl_resource * resource)
{
    struct xdg_surface * xdg_surface = wl_resource_get_user_data(resource);

    wl_list_remove(&xdg_surface->surface_destroy_listener.link);
    window_finalize(&xdg_surface->window);
    free(xdg_surface);
}

struct xdg_surface * xdg_surface_new(struct wl_client * client,
                                     uint32_t version, uint32_t id,
                                     struct swc_surface * surface)
{
    struct xdg_surface * xdg_surface;

    xdg_surface = malloc(sizeof *xdg_surface);

    if (!xdg_surface)
        goto error0;

    xdg_surface->resource = wl_resource_create(client, &xdg_surface_interface,
                                               version, id);

    if (!xdg_surface->resource)
        goto error1;

    window_initialize(&xdg_surface->window, &xdg_surface_window_impl, surface);
    xdg_surface->surface_destroy_listener.notify = &handle_surface_destroy;
    wl_array_init(&xdg_surface->states);
    wl_resource_add_destroy_listener(surface->resource,
                                     &xdg_surface->surface_destroy_listener);
    wl_resource_set_implementation(xdg_surface->resource,
                                   &xdg_surface_implementation,
                                   xdg_surface, &destroy_xdg_surface);
    window_manage(&xdg_surface->window);

    return xdg_surface;

  error1:
    free(xdg_surface);
  error0:
    return NULL;
}

