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
#include "event.h"
#include "internal.h"
#include "keyboard.h"
#include "seat.h"
#include "swc.h"
#include "util.h"
#include "view.h"

#include <stdlib.h>
#include <string.h>

#define INTERNAL(w) ((struct window *) (w))

static void handle_window_enter(struct wl_listener * listener, void * data)
{
    struct swc_event * event = data;
    struct swc_input_focus_event_data * event_data = event->data;
    struct window * window;

    if (event->type != SWC_INPUT_FOCUS_EVENT_CHANGED)
        return;

    if (!event_data->new || !(window = event_data->new->window))
        return;

    swc_send_event(&window->base.event_signal, SWC_WINDOW_ENTERED, NULL);
}

struct wl_listener window_enter_listener = {
    .notify = &handle_window_enter
};

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
void swc_window_focus(struct swc_window * base)
{
    struct window * window = INTERNAL(base);
    struct swc_surface * new_focus = window ? INTERNAL(window)->surface : NULL,
                       * old_focus = swc.seat->keyboard->focus.surface;

    /* If the keyboard already has a focused window, and we are changing the
     * focus to either NULL, or a window with a different implementation, set
     * the focus of the previous focus window's implementation to NULL. */
    if (old_focus && old_focus->window
        && !(window && window->impl == INTERNAL(old_focus->window)->impl)
        && INTERNAL(old_focus->window)->impl->focus)
    {
        INTERNAL(old_focus->window)->impl->focus(NULL);
    }

    if (window && window->impl->focus)
        window->impl->focus(window);

    swc_keyboard_set_focus(swc.seat->keyboard, new_focus);
}

EXPORT
void swc_window_set_geometry(struct swc_window * base,
                             const struct swc_rectangle * geometry)
{
    struct window * window = INTERNAL(base);

    if (window->impl->configure)
        window->impl->configure(window, geometry);

    swc_view_move(window->surface->view, geometry->x, geometry->y);
}

EXPORT
void swc_window_set_border(struct swc_window * window,
                           uint32_t border_color, uint32_t border_width)
{
    struct swc_surface * surface = INTERNAL(window)->surface;

    swc_compositor_surface_set_border_color(surface, border_color);
    swc_compositor_surface_set_border_width(surface, border_width);
}

bool window_initialize(struct window * window, const struct window_impl * impl,
                       struct swc_surface * surface)
{
    DEBUG("Initializing window, %p\n", window);

    window->base.title = NULL;
    window->base.class = NULL;
    window->base.state = SWC_WINDOW_STATE_WITHDRAWN;
    window->base.parent = NULL;
    wl_signal_init(&window->base.event_signal);
    window->surface = surface;
    window->impl = impl;

    surface->window = window;
    swc_compositor_add_surface(surface);

    swc.manager->new_window(&window->base);

    return true;
}

void window_finalize(struct window * window)
{
    DEBUG("Finalizing window, %p\n", window);

    swc_send_event(&window->base.event_signal, SWC_WINDOW_DESTROYED, NULL);
    swc_compositor_remove_surface(window->surface);
    window->surface->window = NULL;
    free(window->base.title);
    free(window->base.class);
}

void window_set_title(struct window * window, const char * title, size_t length)
{
    free(window->base.title);
    window->base.title = strndup(title, length);
    swc_send_event(&window->base.event_signal, SWC_WINDOW_TITLE_CHANGED, NULL);
}

void window_set_class(struct window * window, const char * class)
{
    free(window->base.class);
    window->base.class = strdup(class);
    swc_send_event(&window->base.event_signal, SWC_WINDOW_CLASS_CHANGED, NULL);
}

void window_set_state(struct window * window, uint32_t state)
{
    window->base.state = state;
    swc_send_event(&window->base.event_signal, SWC_WINDOW_STATE_CHANGED, NULL);
}

void window_set_parent(struct window * window, struct window * parent)
{
    if (window->base.parent == &parent->base)
        return;

    window->base.parent = &parent->base;
    swc_send_event(&window->base.event_signal, SWC_WINDOW_PARENT_CHANGED, NULL);
}

