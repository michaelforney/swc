/* swc: libswc/wayland_buffer.c
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

#include "wayland_buffer.h"
#include "buffer.h"
#include "internal.h"
#include "shm.h"
#include "util.h"

#include <wld/wld.h>
#include <wld/pixman.h>

struct wayland_buffer
{
    struct swc_buffer base;
    struct wl_resource * resource;
    struct wl_listener destroy_listener;
};

static void destroy(struct wl_client * client, struct wl_resource * resource)
{
    wl_resource_destroy(resource);
}

static const struct wl_buffer_interface buffer_implementation = {
    .destroy = &destroy
};

/* NOTE: Needed because the implementation for SHM buffers comes from
 *       libwayland-server. */
static void handle_buffer_destroy(struct wl_listener * listener, void * data)
{
    struct wayland_buffer * buffer
        = CONTAINER_OF(listener, typeof(*buffer), destroy_listener);

    swc_buffer_finalize(&buffer->base);
    free(buffer);
}

static inline uint32_t format_shm_to_wld(uint32_t format)
{
    switch (format)
    {
        case WL_SHM_FORMAT_ARGB8888:
            return WLD_FORMAT_ARGB8888;
        case WL_SHM_FORMAT_XRGB8888:
            return WLD_FORMAT_XRGB8888;
        default:
            return format;
    }
}

struct swc_buffer * swc_wayland_buffer_get(struct wl_resource * resource)
{
    if (wl_resource_instance_of(resource, &wl_buffer_interface,
                                &buffer_implementation))
    {
        return wl_resource_get_user_data(resource);
    }

    struct wl_listener * listener;
    struct wayland_buffer * buffer;

    listener = wl_resource_get_destroy_listener(resource,
                                                &handle_buffer_destroy);

    if (listener)
    {
        buffer = CONTAINER_OF(listener, typeof(*buffer), destroy_listener);

        return &buffer->base;
    }

    struct wl_shm_buffer * shm_buffer;
    struct wld_buffer * wld = NULL;

    if ((shm_buffer = wl_shm_buffer_get(resource)))
    {
        union wld_object object = {
            .ptr = wl_shm_buffer_get_data(shm_buffer)
        };

        wld = wld_import_buffer
            (swc.shm->context, WLD_OBJECT_DATA, object,
             wl_shm_buffer_get_width(shm_buffer),
             wl_shm_buffer_get_height(shm_buffer),
             format_shm_to_wld(wl_shm_buffer_get_format(shm_buffer)),
             wl_shm_buffer_get_stride(shm_buffer));
    }

    if (!wld)
        goto error0;

    if (!(buffer = malloc(sizeof *buffer)))
        goto error1;

    swc_buffer_initialize(&buffer->base, wld);
    buffer->resource = resource;
    buffer->destroy_listener.notify = &handle_buffer_destroy;
    wl_resource_add_destroy_listener(resource,
                                     &buffer->destroy_listener);

    return &buffer->base;

  error1:
    wld_destroy_buffer(wld);
  error0:
    return NULL;
}

static void destroy_buffer(struct wl_resource * resource)
{
    struct wayland_buffer * buffer = wl_resource_get_user_data(resource);

    swc_buffer_finalize(&buffer->base);
    free(buffer);
}

struct swc_buffer * swc_wayland_buffer_new
    (struct wl_client * client, uint32_t id, struct wld_buffer * wld)
{
    struct wayland_buffer * buffer;

    buffer = malloc(sizeof *buffer);

    if (!buffer)
        goto error0;

    buffer->resource = wl_resource_create(client, &wl_buffer_interface, 1, id);

    if (!buffer->resource)
        goto error1;

    wl_resource_set_implementation(buffer->resource, &buffer_implementation,
                                   buffer, &destroy_buffer);
    swc_buffer_initialize(&buffer->base, wld);

    return &buffer->base;

  error1:
    free(buffer);
  error0:
    wl_client_post_no_memory(client);
    return NULL;
}

void swc_wayland_buffer_release(struct swc_buffer * buffer)
{
    wl_buffer_send_release(((struct wayland_buffer *) buffer)->resource);
}

