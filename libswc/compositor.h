/* swc: libswc/compositor.h
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

#ifndef SWC_COMPOSITOR_H
#define SWC_COMPOSITOR_H

#include <stdbool.h>

struct swc_surface;

struct swc_compositor
{
    const struct swc_pointer_handler * pointer_handler;
};

bool swc_compositor_initialize();
void swc_compositor_finalize();

bool swc_compositor_add_surface(struct swc_surface * surface);
bool swc_compositor_remove_surface(struct swc_surface * surface);

void swc_compositor_surface_show(struct swc_surface * surface);
void swc_compositor_surface_hide(struct swc_surface * surface);
void swc_compositor_surface_set_border_color(struct swc_surface * surface,
                                             uint32_t color);
void swc_compositor_surface_set_border_width(struct swc_surface * surface,
                                             uint32_t width);

#endif

