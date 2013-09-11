/* swc: drm_buffer.c
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

#include "drm_buffer.h"

#include <stdlib.h>
#include <wayland-server.h>
#include <wayland-drm-server-protocol.h>
#include <wld/wld.h>

static void destroy(struct wl_client * client, struct wl_resource * resource)
{
    wl_resource_destroy(resource);
}

static const struct wl_buffer_interface drm_buffer_implementation = {
    .destroy = &destroy
};

static void buffer_destroy(struct wl_resource * resource)
{
    struct swc_drm_buffer * buffer = wl_resource_get_user_data(resource);

    wld_destroy_drawable(buffer->drawable);
    free(buffer);
}

struct swc_drm_buffer * swc_drm_buffer_new
    (struct wl_client * client, uint32_t id, struct wld_drawable * drawable)
{
    struct swc_drm_buffer * buffer;

    buffer = malloc(sizeof *buffer);

    if (!buffer)
        return NULL;

    buffer->resource = wl_resource_create(client, &wl_buffer_interface, 1, id);
    wl_resource_set_implementation(buffer->resource, &drm_buffer_implementation,
                                   buffer, &buffer_destroy);
    buffer->drawable = drawable;

    return buffer;
}

struct swc_drm_buffer * swc_drm_buffer_get(struct wl_resource * resource)
{
    if (wl_resource_instance_of(resource, &wl_buffer_interface,
                                &drm_buffer_implementation))
    {
        return wl_resource_get_user_data(resource);
    }
    else
        return NULL;
}

