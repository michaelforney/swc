/* swc: libswc/window.c
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
#include "window.h"
#include "compositor.h"
#include "compositor_surface.h"
#include "internal.h"
#include "private.h"

#include <stdlib.h>
#include <string.h>

static void handle_window_enter(struct wl_listener * listener, void * data)
{
    struct swc_event * event = data;
    struct swc_input_focus_event_data * event_data = event->data;
    struct swc_window * window;

    if (event->type != SWC_INPUT_FOCUS_EVENT_CHANGED)
        return;

    if (!event_data->new || !(window = swc_window_get(event_data->new)))
        return;

    swc_send_event(&window->event_signal, SWC_WINDOW_ENTERED, NULL);
}

static struct wl_listener window_enter_listener = {
    .notify = &handle_window_enter
};
struct wl_listener * swc_window_enter_listener = &window_enter_listener;

EXPORT
void swc_window_show(struct swc_window * window)
{
    swc_compositor_surface_show(INTERNAL(window)->surface);
}

EXPORT
void swc_window_hide(struct swc_window * window)
{
    swc_compositor_surface_hide(INTERNAL(window)->surface);
}

EXPORT
void swc_window_focus(struct swc_window * window)
{
    if (INTERNAL(window)->impl->focus)
        INTERNAL(window)->impl->focus(window);

    swc_keyboard_set_focus(&compositor->seat.keyboard,
                           INTERNAL(window)->surface);
}

EXPORT
void swc_window_set_geometry(struct swc_window * window,
                             const struct swc_rectangle * geometry)
{
    if (INTERNAL(window)->impl->configure)
        INTERNAL(window)->impl->configure(window, geometry);

    swc_surface_move(INTERNAL(window)->surface, geometry->x, geometry->y);
}

EXPORT
void swc_window_set_border(struct swc_window * window,
                           uint32_t border_color, uint32_t border_width)
{
    struct swc_surface * surface = INTERNAL(window)->surface;

    swc_compositor_surface_set_border_color(surface, border_color);
    swc_compositor_surface_set_border_width(surface, border_width);
}

static void handle_surface_destroy(struct wl_listener * listener, void * data)
{
    struct swc_window * window = CONTAINER_OF_INTERNAL
        (listener, window, surface_destroy_listener);

    swc_send_event(&window->event_signal, SWC_WINDOW_DESTROYED, NULL);
    free(window);
}

EXPORT
bool swc_window_initialize(struct swc_window * window,
                           const struct swc_window_impl * impl,
                           struct swc_surface * surface)
{
    window->title = NULL;
    window->class = NULL;
    window->state = SWC_WINDOW_STATE_WITHDRAWN;
    wl_signal_init(&window->event_signal);
    INTERNAL(window)->surface = surface;
    INTERNAL(window)->surface_destroy_listener.notify = &handle_surface_destroy;
    INTERNAL(window)->impl = impl;

    wl_resource_add_destroy_listener
        (surface->resource, &INTERNAL(window)->surface_destroy_listener);
    swc_surface_set_class(surface, &compositor->compositor_class);

    swc_manager->new_window(window);

    return true;
}

struct swc_window * swc_window_get(struct swc_surface * surface)
{
    struct wl_listener * listener;

    listener = wl_resource_get_destroy_listener(surface->resource,
                                                &handle_surface_destroy);

    return listener ? CONTAINER_OF_INTERNAL(listener, window,
                                            surface_destroy_listener)
                    : NULL;
}

void swc_window_set_title(struct swc_window * window,
                          const char * title, size_t length)
{
    free(window->title);
    window->title = strndup(title, length);
    swc_send_event(&window->event_signal, SWC_WINDOW_TITLE_CHANGED, NULL);
}

void swc_window_set_class(struct swc_window * window, const char * class)
{
    free(window->class);
    window->class = strdup(class);
    swc_send_event(&window->event_signal, SWC_WINDOW_CLASS_CHANGED, NULL);
}

void swc_window_set_state(struct swc_window * window, uint32_t state)
{
    window->state = state;
    swc_send_event(&window->event_signal, SWC_WINDOW_STATE_CHANGED, NULL);
}

