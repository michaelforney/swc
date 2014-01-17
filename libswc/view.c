/* swc: libswc/view.c
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

#include "view.h"
#include "buffer.h"
#include "event.h"
#include "internal.h"
#include "screen.h"
#include "util.h"

#include <wld/wld.h>

static void update_screens(struct swc_view * view)
{
    struct swc_view_event_data data = { .view = view };
    uint32_t old = view->screens, new = 0;
    struct swc_screen_internal * screen;

    if (view->visible)
    {
        wl_list_for_each(screen, &swc.screens, link)
        {
            if (swc_rectangle_overlap(&screen->base.geometry, &view->geometry))
                new |= swc_screen_mask(screen);
        }
    }

    if (new == old)
        return;

    view->screens = new;

    data.screens_changed.entered = new & ~old;
    data.screens_changed.left = old & ~new;
    swc_send_event(&view->event_signal, SWC_VIEW_EVENT_SCREENS_CHANGED, &data);
}

static void set_size(struct swc_view * view, uint32_t width, uint32_t height)
{
    if (view->geometry.width != width || view->geometry.height != height)
    {
        struct swc_view_event_data data = { .view = view };

        if (view->impl->resize)
            view->impl->resize(view);

        view->geometry.width = width;
        view->geometry.height = height;
        update_screens(view);

        swc_send_event(&view->event_signal, SWC_VIEW_EVENT_RESIZED, &data);
    }
}

static void handle_buffer_destroy(struct wl_listener * listener, void * data)
{
    struct swc_view * view
        = CONTAINER_OF(listener, typeof(*view), buffer_destroy_listener);

    view->impl->attach(view, NULL);
    view->buffer = NULL;
    set_size(view, 0, 0);
}

void swc_view_initialize(struct swc_view * view,
                         const struct swc_view_impl * impl)
{
    view->impl = impl;
    view->visible = true;
    view->geometry.x = 0;
    view->geometry.y = 0;
    view->geometry.width = 0;
    view->geometry.height = 0;
    view->buffer = NULL;
    view->buffer_destroy_listener.notify = &handle_buffer_destroy;
    view->screens = 0;
    wl_signal_init(&view->event_signal);
}

void swc_view_finalize(struct swc_view * view)
{
    if (view->buffer)
        wl_list_remove(&view->buffer_destroy_listener.link);
}

bool swc_view_attach(struct swc_view * view, struct swc_buffer * buffer)
{
    if (view->impl->attach(view, buffer))
    {
        if (view->buffer)
            wl_list_remove(&view->buffer_destroy_listener.link);

        if (buffer)
        {
            wl_signal_add(&buffer->destroy_signal,
                          &view->buffer_destroy_listener);
            set_size(view, buffer->wld->width, buffer->wld->height);
        }
        else
            set_size(view, 0, 0);

        view->buffer = buffer;
        return true;
    }
    else
        return false;
}

bool swc_view_update(struct swc_view * view)
{
    return view->impl->update(view);
}

bool swc_view_move(struct swc_view * view, int32_t x, int32_t y)
{
    struct swc_view_event_data data = { .view = view };

    if (x == view->geometry.x && y == view->geometry.y)
        return true;

    if (!view->impl->move || !view->impl->move(view, x, y))
        return false;

    view->geometry.x = x;
    view->geometry.y = y;
    update_screens(view);
    swc_send_event(&view->event_signal, SWC_VIEW_EVENT_MOVED, &data);

    return true;
}

void swc_view_set_visibility(struct swc_view * view, bool visible)
{
    view->visible = visible;
    update_screens(view);
}

void swc_view_frame(struct swc_view * view, uint32_t time)
{
    struct swc_view_event_data data = { .view = view, .frame = { time } };

    swc_send_event(&view->event_signal, SWC_VIEW_EVENT_FRAME, &data);
}

