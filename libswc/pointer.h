/* swc: libswc/pointer.h
 *
 * Copyright (c) 2013, 2014 Michael Forney
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

#ifndef SWC_POINTER_H
#define SWC_POINTER_H

#include "input_focus.h"
#include "surface.h"
#include "view.h"

#include <wayland-server.h>
#include <pixman.h>

struct swc_pointer;

struct swc_pointer_handler
{
    bool (* motion)(struct swc_pointer_handler * handler, uint32_t time,
                    wl_fixed_t x, wl_fixed_t y);
    bool (* button)(struct swc_pointer_handler * handler, uint32_t time,
                    uint32_t button, uint32_t state);
    bool (* axis)(struct swc_pointer_handler * handler, uint32_t time,
                  enum wl_pointer_axis axis, wl_fixed_t amount);
};

struct swc_pointer
{
    struct swc_input_focus focus;
    struct swc_input_focus_handler focus_handler;

    struct
    {
        struct swc_view view;
        struct wl_listener view_listener;
        struct swc_surface * surface;
        struct wl_listener destroy_listener;
        struct wld_buffer * buffer;

        /* Used for cursors set with swc_pointer_set_cursor */
        struct wld_buffer * internal_buffer;

        struct
        {
            int32_t x, y;
        } hotspot;
    } cursor;

    struct swc_pointer_handler * handler;

    wl_fixed_t x, y;
    pixman_region32_t region;
};

bool swc_pointer_initialize(struct swc_pointer * pointer);
void swc_pointer_finalize(struct swc_pointer * pointer);
void swc_pointer_set_focus(struct swc_pointer * pointer,
                           struct swc_surface * surface);
void swc_pointer_set_region(struct swc_pointer * pointer,
                            pixman_region32_t * region);
void swc_pointer_set_cursor(struct swc_pointer * pointer, uint32_t id);

struct wl_resource * swc_pointer_bind(struct swc_pointer * pointer,
                                      struct wl_client * client, uint32_t id);
void swc_pointer_handle_button(struct swc_pointer * pointer, uint32_t time,
                               uint32_t button, uint32_t state);
void swc_pointer_handle_axis(struct swc_pointer * pointer, uint32_t time,
                             uint32_t axis, wl_fixed_t amount);
void swc_pointer_handle_relative_motion
    (struct swc_pointer * pointer, uint32_t time, wl_fixed_t dx, wl_fixed_t dy);

#endif

