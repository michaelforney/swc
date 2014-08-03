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
#include "event.h"
#include "internal.h"
#include "screen.h"
#include "util.h"

#include <wld/wld.h>

void view_initialize(struct view * view, const struct view_impl * impl)
{
    view->impl = impl;
    view->geometry.x = 0;
    view->geometry.y = 0;
    view->geometry.width = 0;
    view->geometry.height = 0;
    view->buffer = NULL;
    view->screens = 0;
    wl_signal_init(&view->event_signal);
}

void view_finalize(struct view * view)
{
    if (view->buffer)
        wld_buffer_unreference(view->buffer);
}

int view_attach(struct view * view, struct wld_buffer * buffer)
{
    int ret;

    if ((ret = view->impl->attach(view, buffer)) == 0)
    {
        if (view->buffer)
            wld_buffer_unreference(view->buffer);

        if (buffer)
            wld_buffer_reference(buffer);

        view->buffer = buffer;
    }

    return ret;
}

bool view_update(struct view * view)
{
    return view->impl->update(view);
}

bool view_move(struct view * view, int32_t x, int32_t y)
{
    return view->impl->move(view, x, y);
}

bool view_set_position(struct view * view, int32_t x, int32_t y)
{
    struct view_event_data data = { .view = view };

    if (x == view->geometry.x && y == view->geometry.y)
        return false;

    view->geometry.x = x;
    view->geometry.y = y;
    swc_send_event(&view->event_signal, VIEW_EVENT_MOVED, &data);

    return true;
}

bool view_set_size(struct view * view, uint32_t width, uint32_t height)
{
    struct view_event_data data = { .view = view };

    if (view->geometry.width == width && view->geometry.height == height)
        return false;

    view->geometry.width = width;
    view->geometry.height = height;
    swc_send_event(&view->event_signal, VIEW_EVENT_RESIZED, &data);

    return true;
}

bool view_set_size_from_buffer(struct view * view, struct wld_buffer * buffer)
{
    return view_set_size(view, buffer ? buffer->width : 0,
                               buffer ? buffer->height : 0);
}

void view_set_screens(struct view * view, uint32_t screens)
{
    if (view->screens == screens)
        return;

    struct view_event_data data = {
        .view = view,
        .screens_changed = {
            .entered = screens & ~view->screens,
            .left = view->screens & ~screens
        }
    };

    view->screens = screens;
    swc_send_event(&view->event_signal, VIEW_EVENT_SCREENS_CHANGED, &data);
}

void view_update_screens(struct view * view)
{
    uint32_t screens = 0;
    struct screen * screen;

    wl_list_for_each(screen, &swc.screens, link)
    {
        if (swc_rectangle_overlap(&screen->base.geometry, &view->geometry))
            screens |= screen_mask(screen);
    }

    view_set_screens(view, screens);
}

void view_frame(struct view * view, uint32_t time)
{
    struct view_event_data data = { .view = view, .frame = { time } };

    swc_send_event(&view->event_signal, VIEW_EVENT_FRAME, &data);
}

