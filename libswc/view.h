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

#include <stdbool.h>
#include <stdint.h>

struct swc_surface;
struct wl_resource;

struct swc_view
{
    const struct swc_view_impl * impl;
};

/**
 * A view is a set of operations that can be performed on a surface.  This
 * gives the compositor the ability to classify surfaces, treating some
 * specially (for example, a cursor surface).
 */
struct swc_view_impl
{
    /* Called when a surface is removed from the view. */
    void (* remove)(struct swc_surface * surface);

    /* Called when a new buffer is attached to a surface. */
    void (* attach)(struct swc_surface * surface,
                    struct wl_resource * resource);

    /* Called after a surface requests a commit. */
    void (* update)(struct swc_surface * surface);

    /* Moves the surface to the specified coordinates. */
    void (* move)(struct swc_surface * surface, int32_t x, int32_t y);
};

void swc_view_initialize(struct swc_view * view,
                         const struct swc_view_impl * impl);

void swc_view_finalize(struct swc_view * view);

#endif

