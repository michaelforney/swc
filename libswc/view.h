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

struct swc_buffer;

enum swc_view_event
{
    /* Sent when the view has displayed the next frame. */
    SWC_VIEW_EVENT_FRAME,
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
    };
};

struct swc_view
{
    const struct swc_view_impl * impl;

    struct wl_signal event_signal;
};

struct swc_view_impl
{
    /* Called when a source is removed from the view. */
    void (* remove)(struct swc_view * view);

    /* Called when a new buffer is attached to the view. */
    void (* attach)(struct swc_view * view, struct swc_buffer * buffer);

    /* Called when the view should present a new frame. */
    void (* update)(struct swc_view * view);

    /* Move the view to the specified coordinates. */
    void (* move)(struct swc_view * view, int32_t x, int32_t y);
};

void swc_view_initialize(struct swc_view * view,
                         const struct swc_view_impl * impl);

void swc_view_finalize(struct swc_view * view);

void swc_view_frame(struct swc_view * view, uint32_t time);

#endif

