/* swc: libswc/xdg_shell.c
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

#include "xdg_shell.h"
#include "internal.h"
#include "xdg_popup.h"
#include "xdg_surface.h"

#include <assert.h>
#include <wayland-server.h>
#include "protocol/xdg-shell-server-protocol.h"

#define XDG_SHELL_VERSION 5

static_assert(XDG_SHELL_VERSION == XDG_SHELL_VERSION_CURRENT,
              "xdg_shell implementation does not match protocol version");

static struct
{
    struct wl_global * global;
} shell;

static void destroy(struct wl_client * client, struct wl_resource * resource)
{
    wl_resource_destroy(resource);
}

static void use_unstable_version(struct wl_client * client,
                                 struct wl_resource * resource, int32_t version)
{
}

static void get_xdg_surface(struct wl_client * client,
                            struct wl_resource * resource, uint32_t id,
                            struct wl_resource * surface_resource)
{
    struct surface * surface = wl_resource_get_user_data(surface_resource);
    struct xdg_surface * xdg_surface;

    xdg_surface = xdg_surface_new(client, wl_resource_get_version(resource), id,
                                  surface);

    if (!xdg_surface)
        wl_resource_post_no_memory(resource);
}

static void get_xdg_popup(struct wl_client * client,
                          struct wl_resource * resource, uint32_t id,
                          struct wl_resource * surface_resource,
                          struct wl_resource * parent_resource,
                          struct wl_resource * seat_resource,
                          uint32_t serial, int32_t x, int32_t y)
{
    struct surface * surface = wl_resource_get_user_data(surface_resource);
    struct surface * parent = wl_resource_get_user_data(parent_resource);
    struct xdg_popup * popup;

    popup = xdg_popup_new(client, wl_resource_get_version(resource), id,
                          surface, parent, x, y);

    if (!popup)
        wl_resource_post_no_memory(resource);
}

static void pong(struct wl_client * client, struct wl_resource * resource,
                 uint32_t serial)
{
}

static const struct xdg_shell_interface shell_implementation = {
    .destroy = &destroy,
    .use_unstable_version = &use_unstable_version,
    .get_xdg_surface = &get_xdg_surface,
    .get_xdg_popup = &get_xdg_popup,
    .pong = &pong
};

static int unversioned_dispatch(const void * implementation, void * target,
                                uint32_t opcode,
                                const struct wl_message * message,
                                union wl_argument * arguments)
{
    struct wl_resource * resource = target;

    if (opcode != 1)
    {
        wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "use_unstable_version must be called first");
        return 0;
    }

    if (arguments[0].i != XDG_SHELL_VERSION)
    {
        wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "incompatible xdg_shell versions, "
                               "server: %d, client: %d",
                               XDG_SHELL_VERSION, arguments[0].i);
        return 0;
    }

    wl_resource_set_implementation(resource, &shell_implementation, NULL, NULL);
    return 1;
}

static void bind_shell(struct wl_client * client, void * data,
                       uint32_t version, uint32_t id)
{
    struct wl_resource * resource;

    if (version > 1)
        version = 1;

    resource = wl_resource_create(client, &xdg_shell_interface, version, id);
    wl_resource_set_dispatcher(resource, &unversioned_dispatch,
                               NULL, NULL, NULL);
}

bool xdg_shell_initialize(void)
{
    shell.global = wl_global_create(swc.display, &xdg_shell_interface, 1,
                                    NULL, &bind_shell);

    return shell.global;
}

void xdg_shell_finalize(void)
{
    wl_global_destroy(shell.global);
}

