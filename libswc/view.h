/* swc: libswc/view.h
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

#ifndef SWC_VIEW_H
#define SWC_VIEW_H

#include "swc.h"

enum swc_view_event
{
    /* Sent when the view has displayed the next frame. */
    SWC_VIEW_EVENT_FRAME,

    /* Sent when the origin of the view has moved. */
    SWC_VIEW_EVENT_MOVED,

    /* Sent when the view's size changes. This occurs when a buffer of
     * different dimensions is attached to the view. */
    SWC_VIEW_EVENT_RESIZED,

    /* Sent when the set of screens the view is visible on changes. */
    SWC_VIEW_EVENT_SCREENS_CHANGED
};

struct swc_view_event_data
{
    struct swc_view * view;
    union
    {
        struct
        {
            uint32_t time;
        } frame;

        struct
        {
            uint32_t left, entered;
        } screens_changed;
    };
};

struct swc_view
{
    const struct swc_view_impl * impl;

    struct wl_signal event_signal;
    bool visible;
    uint32_t screens;

    struct swc_rectangle geometry;
    struct swc_buffer * buffer;
    struct wl_listener buffer_destroy_listener;
};

struct swc_view_impl
{
    /* Called when the view should present a new frame. */
    bool (* update)(struct swc_view * view);

    /* Called when a new buffer is attached to the view. */
    bool (* attach)(struct swc_view * view, struct swc_buffer * buffer);

    bool (* move)(struct swc_view * view, int32_t x, int32_t y);

    void (* resize)(struct swc_view * view);

    /* Called when a source is removed from the view. */
    void (* remove)(struct swc_view * view);
};

void swc_view_initialize(struct swc_view * view,
                         const struct swc_view_impl * impl);

void swc_view_finalize(struct swc_view * view);

bool swc_view_attach(struct swc_view * view, struct swc_buffer * buffer);
bool swc_view_update(struct swc_view * view);
bool swc_view_move(struct swc_view * view, int32_t x, int32_t y);
void swc_view_set_visibility(struct swc_view * view, bool visible);
void swc_view_frame(struct swc_view * view, uint32_t time);

#endif

