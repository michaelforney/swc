/* swc: compositor_surface.h
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

#ifndef SWC_COMPOSITOR_SURFACE_H
#define SWC_COMPOSITOR_SURFACE_H

#include "surface.h"

#include <wayland-server.h>
#include <pixman.h>

struct swc_compositor_surface_state
{
    struct swc_compositor * compositor;

    /* The box that the surface covers (including it's border). */
    pixman_box32_t extents;

    /* The region that is covered by opaque regions of surfaces above this
     * surface. */
    pixman_region32_t clip;

    struct
    {
        uint32_t width;
        uint32_t color;
        bool damaged;
    } border;

    bool mapped;

    struct wl_listener event_listener;
};

extern const struct swc_surface_class_interface
    swc_compositor_class_implementation;

void swc_compositor_surface_show(struct swc_surface * surface);

void swc_compositor_surface_hide(struct swc_surface * surface);

void swc_compositor_surface_set_border_width(struct swc_surface * surface,
                                             uint32_t width);

void swc_compositor_surface_set_border_color(struct swc_surface * surface,
                                             uint32_t color);

#endif

