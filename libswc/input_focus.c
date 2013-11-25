/* swc: input_focus.c
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

#include "input_focus.h"
#include "event.h"
#include "surface.h"
#include "util.h"

static inline void focus(struct swc_input_focus * input_focus,
                         struct swc_surface * surface,
                         struct wl_resource * resource)
{
    if (surface)
    {
        wl_resource_add_destroy_listener
            (surface->resource, &input_focus->surface_destroy_listener);
    }

    if (resource)
        input_focus->handler->enter(input_focus->handler, resource, surface);

    input_focus->surface = surface;
    input_focus->resource = resource;
}

static inline void unfocus(struct swc_input_focus * input_focus)
{
    if (input_focus->surface)
        wl_list_remove(&input_focus->surface_destroy_listener.link);

    if (input_focus->resource)
    {
        input_focus->handler->leave(input_focus->handler, input_focus->resource,
                                    input_focus->surface);
    }
}

static void handle_focus_surface_destroy(struct wl_listener * listener,
                                         void * data)
{
    struct swc_input_focus * input_focus = CONTAINER_OF
        (listener, typeof(*input_focus), surface_destroy_listener);

    input_focus->surface = NULL;
    input_focus->resource = NULL;
}

bool swc_input_focus_initialize(struct swc_input_focus * input_focus,
                                struct swc_input_focus_handler * handler)
{
    input_focus->resource = NULL;
    input_focus->surface = NULL;
    input_focus->surface_destroy_listener.notify
        = &handle_focus_surface_destroy;
    input_focus->handler = handler;

    wl_list_init(&input_focus->resources);
    wl_signal_init(&input_focus->event_signal);

    return true;
}

void swc_input_focus_finish(struct swc_input_focus * input_focus)
{
    /* XXX: Destroy resources? */
}

void swc_input_focus_add_resource(struct swc_input_focus * input_focus,
                                  struct wl_resource * resource)
{
    /* If this new input resource corresponds to our focus, set it as our
     * focus. */
    if (input_focus->surface)
    {
        struct wl_client * client, * surface_client;

        client = wl_resource_get_client(resource);
        surface_client = wl_resource_get_client(input_focus->surface->resource);

        if (client == surface_client)
        {
            unfocus(input_focus);
            focus(input_focus, input_focus->surface, resource);
        }
    }

    wl_list_insert(&input_focus->resources, wl_resource_get_link(resource));
}

void swc_input_focus_remove_resource(struct swc_input_focus * input_focus,
                                     struct wl_resource * resource)
{
    if (resource == input_focus->resource)
        input_focus->resource = NULL;

    swc_remove_resource(resource);
}

void swc_input_focus_set(struct swc_input_focus * input_focus,
                         struct swc_surface * surface)
{
    struct wl_client * client;
    struct wl_display * display;
    struct wl_resource * resource;
    uint32_t serial;
    struct swc_input_focus_event_data data;

    if (surface == input_focus->surface)
        return;

    data.old = input_focus->surface;

    /* Unfocus previously focused surface. */
    unfocus(input_focus);

    /* Focus new surface, if given. */
    if (surface)
    {
        client = wl_resource_get_client(surface->resource);
        resource = wl_resource_find_for_client(&input_focus->resources, client);

        focus(input_focus, surface, resource);
    }
    else
    {
        input_focus->surface = NULL;
        input_focus->resource = NULL;
    }

    data.new = input_focus->surface;

    swc_send_event(&input_focus->event_signal, SWC_INPUT_FOCUS_EVENT_CHANGED,
                   &data);

    return;
}

