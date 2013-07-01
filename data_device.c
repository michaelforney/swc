/* swc: data_device.c
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

#include "data_device.h"
#include "data.h"
#include "util.h"

static void start_drag(struct wl_client * client, struct wl_resource * resource,
                       struct wl_resource * source_resource,
                       struct wl_resource * origin_resource,
                       struct wl_resource * icon_resource, uint32_t serial)
{
    /* XXX: Implement */
}

static void set_selection(struct wl_client * client,
                          struct wl_resource * resource,
                          struct wl_resource * data_source, uint32_t serial)
{
    struct swc_data_device * data_device = wl_resource_get_user_data(resource);
    struct swc_event event;

    /* Check if this data source is already the current selection. */
    if (data_source == data_device->selection)
        return;

    event.type = SWC_DATA_DEVICE_EVENT_SELECTION_CHANGED;

    if (data_device->selection)
    {
        wl_data_source_send_cancelled(data_device->selection);
        wl_list_remove(&data_device->selection_destroy_listener.link);
    }

    data_device->selection = data_source;

    if (data_source)
    {
        wl_resource_add_destroy_listener
            (data_source, &data_device->selection_destroy_listener);
    }

    wl_signal_emit(&data_device->event_signal, &event);
}

struct wl_data_device_interface data_device_implementation = {
    .start_drag = &start_drag,
    .set_selection = &set_selection
};

static void handle_selection_destroy(struct wl_listener * listener, void * data)
{
    struct swc_data_device * data_device
        = wl_container_of(listener, data_device, selection_destroy_listener);
    struct swc_event event;

    event.type = SWC_DATA_DEVICE_EVENT_SELECTION_CHANGED;
    data_device->selection = NULL;
    wl_signal_emit(&data_device->event_signal, &event);
}

bool swc_data_device_initialize(struct swc_data_device * data_device)
{
    data_device->selection_destroy_listener.notify = &handle_selection_destroy;
    wl_signal_init(&data_device->event_signal);
    wl_list_init(&data_device->resources);

    return true;
}

void swc_data_device_finish(struct swc_data_device * data_device)
{
    struct wl_resource * resource, * tmp;

    wl_list_for_each_safe(resource, tmp, &data_device->resources, link)
        wl_resource_destroy(resource);
}

void swc_data_device_bind(struct swc_data_device * data_device,
                          struct wl_client * client, uint32_t id)
{
    struct wl_resource * resource;

    resource = wl_client_add_object(client, &wl_data_device_interface,
                                    &data_device_implementation, id,
                                    data_device);
    wl_list_insert(&data_device->resources, &resource->link);
    wl_resource_set_destructor(resource, &swc_remove_resource);
}

static struct wl_resource * new_offer(struct wl_resource * resource,
                                      struct wl_client * client,
                                      struct wl_resource * source)
{
    struct wl_resource * offer;

    offer = swc_data_offer_new(client, source);
    wl_data_device_send_data_offer(resource, offer);
    swc_data_send_mime_types(source, offer);

    return offer;
}

void swc_data_device_offer_selection(struct swc_data_device * data_device,
                                     struct wl_client * client)
{
    struct wl_resource * resource;
    struct wl_resource * offer;

    /* Look for the client's data_device resource. */
    resource = wl_resource_find_for_client(&data_device->resources, client);

    /* If the client does not have a data device, there is nothing to do. */
    if (!resource)
        return;

    /* If we don't have a selection, send NULL to the client. */
    offer = data_device->selection
        ? new_offer(resource, client, data_device->selection) : NULL;

    wl_data_device_send_selection(resource, offer);
}

