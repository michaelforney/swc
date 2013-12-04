/* swc: data_device_manager.c
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

#include "data_device_manager.h"
#include "data.h"
#include "data_device.h"
#include "internal.h"
#include "seat.h"

static struct
{
    struct wl_global * global;
} data_device_manager;

static void create_data_source(struct wl_client * client,
                               struct wl_resource * resource, uint32_t id)
{
    struct wl_resource * data_source;

    data_source = swc_data_source_new(client, id);

    if (!data_source)
        wl_resource_post_no_memory(resource);
}

static void get_data_device(struct wl_client * client,
                            struct wl_resource * resource, uint32_t id,
                            struct wl_resource * seat_resource)
{
    swc_data_device_bind(swc.seat->data_device, client, id);
}

static struct wl_data_device_manager_interface
    data_device_manager_implementation = {
    .create_data_source = &create_data_source,
    .get_data_device = &get_data_device
};

static void bind_data_device_manager(struct wl_client * client, void * data,
                                     uint32_t version, uint32_t id)
{
    struct wl_resource * resource;

    resource = wl_resource_create(client, &wl_data_device_manager_interface,
                                  1, id);
    wl_resource_set_implementation
        (resource, &data_device_manager_implementation, NULL, NULL);
}

bool swc_data_device_manager_initialize()
{
    data_device_manager.global
        = wl_global_create(swc.display, &wl_data_device_manager_interface, 1,
                           NULL, &bind_data_device_manager);

    return data_device_manager.global != NULL;
}

void swc_data_device_manager_finalize()
{
    wl_global_destroy(data_device_manager.global);
}

