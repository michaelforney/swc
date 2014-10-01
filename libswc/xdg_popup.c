/* swc: libswc/xdg_popup.c
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

#include "xdg_popup.h"
#include "compositor.h"
#include "surface.h"
#include "util.h"
#include "window.h"
#include "protocol/xdg-shell-server-protocol.h"

#include <stdlib.h>

struct xdg_popup
{
    struct wl_resource * resource;
    struct compositor_view * view;
    struct wl_listener surface_destroy_listener;
};

static void destroy(struct wl_client * client, struct wl_resource * resource)
{
    wl_resource_destroy(resource);
}

static const struct xdg_popup_interface xdg_popup_implementation = {
    .destroy = &destroy
};

static void handle_surface_destroy(struct wl_listener * listener, void * data)
{
    struct xdg_popup * popup
        = wl_container_of(listener, popup, surface_destroy_listener);

    wl_resource_destroy(popup->resource);
}

static void destroy_popup(struct wl_resource * resource)
{
    struct xdg_popup * popup = wl_resource_get_user_data(resource);

    wl_list_remove(&popup->surface_destroy_listener.link);
    compositor_view_destroy(popup->view);
    free(popup);
}

struct xdg_popup * xdg_popup_new(struct wl_client * client,
                                 uint32_t version, uint32_t id,
                                 struct swc_surface * surface,
                                 struct swc_surface * parent_surface,
                                 int32_t x, int32_t y)
{
    struct xdg_popup * popup;
    struct compositor_view * parent = compositor_view(parent_surface->view);

    if (!parent)
        goto error0;

    popup = malloc(sizeof *popup);

    if (!popup)
        goto error0;

    popup->resource = wl_resource_create(client, &xdg_popup_interface,
                                         version, id);

    if (!popup->resource)
        goto error1;

    popup->surface_destroy_listener.notify = &handle_surface_destroy;
    wl_resource_add_destroy_listener(surface->resource,
                                     &popup->surface_destroy_listener);
    wl_resource_set_implementation(popup->resource, &xdg_popup_implementation,
                                   popup, &destroy_popup);

    if (!(popup->view = swc_compositor_create_view(surface)))
        goto error2;

    view_move(&popup->view->base,
              parent->base.geometry.x + x, parent->base.geometry.y + y);
    compositor_view_set_parent(popup->view, parent);

    return popup;

  error2:
    wl_resource_destroy(popup->resource);
  error1:
    free(popup);
  error0:
    return NULL;
}

