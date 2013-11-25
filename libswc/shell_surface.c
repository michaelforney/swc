/* swc: libswc/shell_surface.c
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

#include "swc.h"
#include "compositor_surface.h"
#include "internal.h"
#include "shell_surface.h"
#include "window.h"

#include <stdlib.h>

struct swc_shell_surface
{
    struct swc_window_internal window;

    struct wl_resource * resource;

    enum
    {
        SHELL_SURFACE_TYPE_UNSPECIFIED,
        SHELL_SURFACE_TYPE_TOPLEVEL,
        SHELL_SURFACE_TYPE_TRANSIENT,
        SHELL_SURFACE_TYPE_FULLSCREEN,
        SHELL_SURFACE_TYPE_POPUP,
        SHELL_SURFACE_TYPE_MAXIMIZED
    } type;
};

static void pong(struct wl_client * client, struct wl_resource * resource,
                 uint32_t serial)
{
}

static void move(struct wl_client * client, struct wl_resource * resource,
                 struct wl_resource * seat_resource, uint32_t serial)
{
}

static void resize(struct wl_client * client, struct wl_resource * resource,
                   struct wl_resource * seat_resource, uint32_t serial,
                   uint32_t edges)
{
}

static void set_toplevel(struct wl_client * client,
                         struct wl_resource * resource)
{
    struct swc_shell_surface * shell_surface
        = wl_resource_get_user_data(resource);

    if (shell_surface->type == SHELL_SURFACE_TYPE_TOPLEVEL)
        return;

    shell_surface->type = SHELL_SURFACE_TYPE_TOPLEVEL;
    swc_window_set_state(&shell_surface->window.base,
                         SWC_WINDOW_STATE_TOPLEVEL);
}

static void set_transient(struct wl_client * client,
                          struct wl_resource * resource,
                          struct wl_resource * parent_resource,
                          int32_t x, int32_t y, uint32_t flags)
{
    struct swc_shell_surface * shell_surface
        = wl_resource_get_user_data(resource);
    struct swc_surface * parent = wl_resource_get_user_data(parent_resource);

    swc_surface_move(shell_surface->window.surface,
                     parent->geometry.x + x, parent->geometry.y + y);
    swc_compositor_surface_show(shell_surface->window.surface);

    /* XXX: Handle transient */
}

static void set_fullscreen(struct wl_client * client,
                           struct wl_resource * resource,
                           uint32_t method, uint32_t framerate,
                           struct wl_resource * output_resource)
{
    /* XXX: Handle fullscreen */
}

static void set_popup(struct wl_client * client, struct wl_resource * resource,
                      struct wl_resource * seat_resource, uint32_t serial,
                      struct wl_resource * parent_resource,
                      int32_t x, int32_t y, uint32_t flags)
{
    struct swc_shell_surface * shell_surface
        = wl_resource_get_user_data(resource);
    struct swc_surface * parent = wl_resource_get_user_data(parent_resource);

    swc_surface_move(shell_surface->window.surface,
                     parent->geometry.x + x, parent->geometry.y + y);
    swc_compositor_surface_show(shell_surface->window.surface);

    /* XXX: Handle popup */
}

static void set_maximized(struct wl_client * client,
                          struct wl_resource * resource,
                          struct wl_resource * output_resource)
{
    /* XXX: Handle maximized */
}

static void set_title(struct wl_client * client, struct wl_resource * resource,
                      const char * title)
{
    struct swc_shell_surface * shell_surface
        = wl_resource_get_user_data(resource);

    swc_window_set_title(&shell_surface->window.base, title, -1);
}

static void set_class(struct wl_client * client, struct wl_resource * resource,
                      const char * class)
{
    struct swc_shell_surface * shell_surface
        = wl_resource_get_user_data(resource);

    swc_window_set_class(&shell_surface->window.base, class);
}

static const struct wl_shell_surface_interface shell_surface_implementation = {
    .pong = &pong,
    .move = &move,
    .resize = &resize,
    .set_toplevel = &set_toplevel,
    .set_transient = &set_transient,
    .set_fullscreen = &set_fullscreen,
    .set_popup = &set_popup,
    .set_maximized = &set_maximized,
    .set_title = &set_title,
    .set_class = &set_class
};

static void configure(struct swc_window * window,
                      const struct swc_rectangle * geometry)
{
    struct swc_shell_surface * shell_surface
        = CONTAINER_OF(window, typeof(*shell_surface), window.base);

    wl_shell_surface_send_configure(shell_surface->resource,
                                    WL_SHELL_SURFACE_RESIZE_NONE,
                                    geometry->width, geometry->height);
}

static const struct swc_window_impl shell_window_impl = {
    .configure = &configure
};

struct swc_shell_surface * swc_shell_surface_new
    (struct wl_client * client, uint32_t id, struct swc_surface * surface)
{
    struct swc_shell_surface * shell_surface;

    shell_surface = malloc(sizeof *shell_surface);

    if (!shell_surface)
        return NULL;

    shell_surface->type = SHELL_SURFACE_TYPE_UNSPECIFIED;
    swc_window_initialize(&shell_surface->window.base,
                          &shell_window_impl, surface);

    shell_surface->resource = wl_resource_create
        (client, &wl_shell_surface_interface, 1, id);
    wl_resource_set_implementation(shell_surface->resource,
                                   &shell_surface_implementation,
                                   shell_surface, NULL);

    return shell_surface;
}

