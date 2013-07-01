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
#include "data_device.h"
#include "data.h"
#include "seat.h"

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
    struct swc_seat * seat = wl_resource_get_user_data(seat_resource);

    printf("data_device_manager.get_data_device\n");

    swc_data_device_bind(&seat->data_device, client, id);
}

static struct wl_data_device_manager_interface
    data_device_manager_implementation = {
    .create_data_source = &create_data_source,
    .get_data_device = &get_data_device
};

static void bind_data_device_manager(struct wl_client * client, void * data,
                                     uint32_t version, uint32_t id)
{
    wl_client_add_object(client, &wl_data_device_manager_interface,
                         &data_device_manager_implementation, id, NULL);
}

void swc_data_device_manager_add_globals(struct wl_display * display)
{
    wl_display_add_global(display, &wl_data_device_manager_interface, NULL,
                          &bind_data_device_manager);
}

