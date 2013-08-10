/* swc: data.c
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

#include "data.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct data
{
    struct wl_array mime_types;
    struct wl_resource * source;
    struct wl_list offers;
};

static void offer_accept(struct wl_client * client,
                         struct wl_resource * offer,
                         uint32_t serial, const char * mime_type)
{
    struct data * data = wl_resource_get_user_data(offer);

    /* Protect against expired data_offers being used. */
    if (data)
        wl_data_source_send_target(data->source, mime_type);
}

static void offer_receive(struct wl_client * client,
                          struct wl_resource * offer,
                          const char * mime_type, int fd)
{
    struct data * data = wl_resource_get_user_data(offer);

    /* Protect against expired data_offers being used. */
    if (data)
    {
        wl_data_source_send_send(data->source, mime_type, fd);
        close(fd);
    }
}

static void offer_destroy(struct wl_client * client,
                          struct wl_resource * offer)
{
    wl_resource_destroy(offer);
}

struct wl_data_offer_interface data_offer_implementation = {
    .accept = &offer_accept,
    .receive = &offer_receive,
    .destroy = &offer_destroy
};

static void source_offer(struct wl_client * client,
                         struct wl_resource * source,
                         const char * mime_type)
{
    struct data * data = wl_resource_get_user_data(source);
    char ** destination;

    destination = wl_array_add(&data->mime_types, sizeof *destination);
    *destination = strdup(mime_type);
}

static void source_destroy(struct wl_client * client,
                           struct wl_resource * source)
{
    wl_resource_destroy(source);
}

struct wl_data_source_interface data_source_implementation = {
    .offer = &source_offer,
    .destroy = &source_destroy
};

static void data_destroy(struct wl_resource * source)
{
    struct data * data = wl_resource_get_user_data(source);
    struct wl_resource * offer;
    char ** mime_type;

    wl_array_for_each(mime_type, &data->mime_types)
        free(*mime_type);
    wl_array_release(&data->mime_types);

    /* After this data_source is destroyed, each of the data_offer objects
     * associated with the data_source has a pointer to a free'd struct. We
     * can't destroy the resources because this results in a segfault on the
     * client when it correctly tries to call data_source.destroy. However, a
     * misbehaving client could still attempt to call accept or receive on the
     * data_offer, which would crash the server.
     *
     * So, we clear the user data on each of the offers to protect us. */
    wl_list_for_each(offer, &data->offers, link)
        wl_resource_set_user_data(offer, NULL);

    free(data);
}

static struct data * data_new()
{
    struct data * data;

    data = malloc(sizeof *data);

    if (!data)
        return NULL;

    wl_array_init(&data->mime_types);
    wl_list_init(&data->offers);

    return data;
}

struct wl_resource * swc_data_source_new(struct wl_client * client, uint32_t id)
{
    struct data * data;

    data = data_new();

    if (!data)
        return NULL;

    /* Add the data source to the client. */
    data->source = wl_client_add_object(client, &wl_data_source_interface,
                                        &data_source_implementation, id, data);

    /* Destroy the data object when the source disappears. */
    wl_resource_set_destructor(data->source, &data_destroy);

    return data->source;
}

struct wl_resource * swc_data_offer_new(struct wl_client * client,
                                        struct wl_resource * source)
{
    struct data * data = wl_resource_get_user_data(source);
    struct wl_resource * offer;

    offer = wl_client_new_object(client, &wl_data_offer_interface,
                                 &data_offer_implementation, data);
    wl_list_insert(&data->offers, wl_resource_get_link(offer));
    wl_resource_set_destructor(offer, &swc_remove_resource);

    return offer;
}

void swc_data_send_mime_types(struct wl_resource * source,
                              struct wl_resource * offer)
{
    struct data * data = wl_resource_get_user_data(source);
    char ** mime_type;

    wl_array_for_each(mime_type, &data->mime_types)
        wl_data_offer_send_offer(offer, *mime_type);
}

