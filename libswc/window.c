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
    struct input_focus_event_data * event_data = event->data;
    struct window * window;

    if (event->type != INPUT_FOCUS_EVENT_CHANGED)
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
    compositor_view_show(INTERNAL(window)->view);
}

EXPORT
void swc_window_hide(struct swc_window * window)
{
    compositor_view_hide(INTERNAL(window)->view);
}

EXPORT
void swc_window_focus(struct swc_window * base)
{
    struct window * window = INTERNAL(base);
    struct swc_surface * new_focus = window ? window->surface : NULL,
                       * old_focus = swc.seat->keyboard->focus.surface;

    /* If the keyboard already has a focused window, and we are changing the
     * focus to either NULL, or a window with a different implementation, set
     * the focus of the previous focus window's implementation to NULL. */
    if (old_focus && old_focus->window
        && !(window && window->impl == INTERNAL(old_focus->window)->impl)
        && old_focus->window->impl->focus)
    {
        old_focus->window->impl->focus(NULL);
    }

    if (window && window->impl->focus)
        window->impl->focus(window);

    keyboard_set_focus(swc.seat->keyboard, new_focus);
}

EXPORT
void swc_window_set_geometry(struct swc_window * base,
                             const struct swc_rectangle * geometry)
{
    struct window * window = INTERNAL(base);

    if (window->impl->configure)
        window->impl->configure(window, geometry);

    view_move(&window->view->base, geometry->x, geometry->y);
}

EXPORT
void swc_window_set_border(struct swc_window * window,
                           uint32_t border_color, uint32_t border_width)
{
    struct compositor_view * view = INTERNAL(window)->view;

    compositor_view_set_border_color(view, border_color);
    compositor_view_set_border_width(view, border_width);
}

static inline void window_begin_interaction
    (struct window * window, struct window_pointer_interaction * interaction,
     struct button_press * button)
{
    if (button)
    {
        interaction->original_handler = button->handler;
        button->handler = &interaction->handler;
    }
    else
        interaction->original_handler = NULL;

    wl_list_insert(&swc.seat->pointer->handlers, &interaction->handler.link);
}

void window_begin_interactive_move(struct window * window,
                                   struct button_press * button)
{
    struct swc_rectangle * geometry = &window->view->base.geometry;
    int32_t px = wl_fixed_to_int(swc.seat->pointer->x),
            py = wl_fixed_to_int(swc.seat->pointer->y);

    window_begin_interaction(window, &window->move.interaction, button);
    window->move.offset.x = geometry->x - px;
    window->move.offset.y = geometry->y - py;
}

void window_begin_interactive_resize(struct window * window, uint32_t edges,
                                     struct button_press * button)
{
    window_begin_interaction(window, &window->resize.interaction, button);

    if (!edges)
    {
        /* TODO: Calculate edges to use */
    }

    window->resize.edges = edges;
}

EXPORT
void swc_window_begin_move(struct swc_window * base)
{
    struct window * window = (struct window *) base;

    window_begin_interactive_move(window, NULL);
}

EXPORT
void swc_window_end_move(struct swc_window * base)
{
    struct window * window = (struct window *) base;

    wl_list_remove(&window->move.interaction.handler.link);
}

EXPORT
void swc_window_begin_resize(struct swc_window * base, uint32_t edges)
{
    struct window * window = (struct window *) base;

    window_begin_interactive_resize(window, edges, NULL);
}

EXPORT
void swc_window_end_resize(struct swc_window * base)
{
    struct window * window = (struct window *) base;

    wl_list_remove(&window->resize.interaction.handler.link);
}

static bool move_motion(struct pointer_handler * handler, uint32_t time,
                        wl_fixed_t fx, wl_fixed_t fy)
{
    struct window * window
        = CONTAINER_OF(handler, typeof(*window), move.interaction.handler);

    view_move(&window->view->base, wl_fixed_to_int(fx) + window->move.offset.x,
                                   wl_fixed_to_int(fy) + window->move.offset.y);

    return true;
}

static bool resize_motion(struct pointer_handler * handler, uint32_t time,
                          wl_fixed_t fx, wl_fixed_t fy)
{
    /* TODO: Implement interactive resizing */

    return true;
}

static bool handle_button(struct pointer_handler * handler, uint32_t time,
                          uint32_t button, uint32_t state)
{
    struct window_pointer_interaction * interaction
        = CONTAINER_OF(handler, typeof(*interaction), handler);

    if (state != WL_POINTER_BUTTON_STATE_RELEASED
        || !interaction->original_handler)
    {
        return false;
    }

    interaction->original_handler->button
        (interaction->original_handler, time, button, state);
    wl_list_remove(&handler->link);

    return true;
}

bool window_initialize(struct window * window, const struct window_impl * impl,
                       struct swc_surface * surface)
{
    DEBUG("Initializing window, %p\n", window);

    window->base.title = NULL;
    window->base.class = NULL;
    window->base.state = SWC_WINDOW_STATE_NONE;
    window->base.parent = NULL;
    wl_signal_init(&window->base.event_signal);

    if (!(window->view = swc_compositor_create_view(surface)))
        return false;

    window->surface = surface;
    window->impl = impl;
    window->move.interaction.handler = (struct pointer_handler) {
        .motion = &move_motion,
        .button = &handle_button
    };
    window->resize.interaction.handler = (struct pointer_handler) {
        .motion = &resize_motion,
        .button = &handle_button
    };

    surface->window = window;

    swc.manager->new_window(&window->base);

    return true;
}

void window_finalize(struct window * window)
{
    DEBUG("Finalizing window, %p\n", window);

    swc_send_event(&window->base.event_signal, SWC_WINDOW_DESTROYED, NULL);
    compositor_view_destroy(window->view);
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

    compositor_view_set_parent(window->view, parent->view);
    window->base.parent = &parent->base;
    swc_send_event(&window->base.event_signal, SWC_WINDOW_PARENT_CHANGED, NULL);
}

