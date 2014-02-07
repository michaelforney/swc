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

enum
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

/**
 * This structure contains data sent along with a view's events.
 *
 * Extra data correspending to the particular event is stored in the
 * corresponding struct inside the union.
 */
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

/**
 * A view represents a component that can display buffers to the user.
 *
 * For example, each output contains a view residing in its framebuffer plane
 * that is used to scan out buffers. Additionally, the compositor creates views
 * dynamically for each surface it displays, compositing them into a single
 * buffer, which it can then attach to the previously mentioned view.
 *
 * This abstraction allows various components of swc to connect in a general
 * way, allowing operations like setting the output view of a surface directly
 * to an output's framebuffer plane, bypassing the compositor.
 */
struct swc_view
{
    const struct swc_view_impl * impl;

    struct wl_signal event_signal;
    bool visible;
    uint32_t screens;

    struct swc_rectangle geometry;

    /**
     * The screen that the view belongs to (for example if framebuffer or
     * cursor plane), or NULL.
     */
    struct screen * screen;

    struct wld_buffer * buffer;
};

/**
 * Every view must have an implementation containing these functions.
 *
 * For descriptions, see the corresponding swc_view_* function.
 */
struct swc_view_impl
{
    bool (* update)(struct swc_view * view);
    bool (* attach)(struct swc_view * view, struct wld_buffer * buffer);
    bool (* move)(struct swc_view * view, int32_t x, int32_t y);

    /**
     * This function indicates to the view that it is about to be resized.
     *
     * The view can use this function to do any necessary damage calculations
     * with the geometry of the view before the resize occurs.
     */
    void (* resize)(struct swc_view * view);
};

/**
 * Initialize a new view with the specified implementation.
 */
void swc_view_initialize(struct swc_view * view,
                         const struct swc_view_impl * impl);

/**
 * Release any resources associated with this view.
 */
void swc_view_finalize(struct swc_view * view);

/**
 * Attach a new buffer to the view.
 *
 * If buffer is NULL, the previous buffer is removed from the view.
 *
 * @return Whether or not the buffer was successfully attached to the view.
 */
bool swc_view_attach(struct swc_view * view, struct wld_buffer * buffer);

/**
 * Display a new frame consisting of the currently attached buffer.
 *
 * @return Whether or not the update succeeds.
 */
bool swc_view_update(struct swc_view * view);

/**
 * Move the view to the specified coordinates, if supported.
 *
 * @return Whether or not the move succeeds.
 */
bool swc_view_move(struct swc_view * view, int32_t x, int32_t y);

/**
 * Set the visibility flag of the view.
 *
 * This retains the view's geometry, but indicates that it will not be shown
 * when the next frame is finished. This is useful when an unmapped or
 * minimized state is desired.
 */
void swc_view_set_visibility(struct swc_view * view, bool visible);

/**
 * Send a new frame event through the view's event signal.
 *
 * This should be called by the view itself when the next frame is visible to
 * the user. If time information is not available, swc_time() can be passed
 * instead.
 */
void swc_view_frame(struct swc_view * view, uint32_t time);

#endif

