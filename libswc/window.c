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

#include "window.h"
#include "compositor.h"
#include "compositor_surface.h"
#include "event.h"
#include "internal.h"
#include "keyboard.h"
#include "seat.h"
#include "swc.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

#define INTERNAL(window) ((struct swc_window_internal *) (window))

static void handle_window_enter(struct wl_listener * listener, void * data)
{
    struct swc_event * event = data;
    struct swc_input_focus_event_data * event_data = event->data;
    struct swc_window * window;

    if (event->type != SWC_INPUT_FOCUS_EVENT_CHANGED)
        return;

    if (!event_data->new || !(window = event_data->new->window))
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
    struct swc_surface * new_focus = window ? INTERNAL(window)->surface : NULL,
                       * old_focus = swc.seat->keyboard->focus.surface;

    /* If the keyboard already has a focused window, and we are changing the
     * focus to either NULL, or a window with a different implementation, set
     * the focus of the previous focus window's implementation to NULL. */
    if (old_focus && old_focus->window
        && !(window && INTERNAL(window)->impl
                    == INTERNAL(old_focus->window)->impl)
        && INTERNAL(old_focus->window)->impl->focus)
    {
        INTERNAL(old_focus->window)->impl->focus(NULL);
    }

    if (window && INTERNAL(window)->impl->focus)
        INTERNAL(window)->impl->focus(window);

    swc_keyboard_set_focus(swc.seat->keyboard, new_focus);
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

bool swc_window_initialize(struct swc_window * window,
                           const struct swc_window_impl * impl,
                           struct swc_surface * surface)
{
    DEBUG("Initializing window, %p\n", window);

    window->title = NULL;
    window->class = NULL;
    window->state = SWC_WINDOW_STATE_WITHDRAWN;
    wl_signal_init(&window->event_signal);
    INTERNAL(window)->surface = surface;
    INTERNAL(window)->impl = impl;

    surface->window = window;
    swc_surface_set_view(surface, &swc.compositor->compositor_view);

    swc.manager->new_window(window);

    return true;
}

void swc_window_finalize(struct swc_window * window)
{
    DEBUG("Finalizing window, %p\n", window);

    swc_send_event(&window->event_signal, SWC_WINDOW_DESTROYED, NULL);
    swc_surface_set_view(INTERNAL(window)->surface, NULL);
    INTERNAL(window)->surface->window = NULL;
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

