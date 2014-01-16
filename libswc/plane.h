/* swc: plane.h
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

#ifndef SWC_PLANE_H
#define SWC_PLANE_H

#include "util.h"

struct swc_plane;

struct swc_plane_interface
{
    bool (* initialize)(struct swc_plane * plane);
    void * (* create_buffer)(struct swc_plane * plane);
    void (* destroy_buffer)(struct swc_plane * plane, void * data);
    struct wld_buffer * (* get_buffer)(void * data);
    bool (* flip)(struct swc_plane * plane);
    bool (* move)(struct swc_plane * plane, int32_t x, int32_t y);
};

struct swc_plane
{
    const struct swc_plane_interface * interface;
    struct swc_output * output;

    struct swc_double_buffer double_buffer;

    /* Relative to the output's origin. */
    int32_t x, y;
};

bool swc_plane_initialize(struct swc_plane * plane,
                          const struct swc_plane_interface * interface,
                          struct swc_output * output);

bool swc_plane_flip(struct swc_plane * plane);

bool swc_plane_move(struct swc_plane * plane, int32_t x, int32_t y);

struct wld_buffer * swc_plane_get_buffer(struct swc_plane * plane);

extern const struct swc_plane_interface swc_framebuffer_plane;
extern const struct swc_plane_interface swc_cursor_plane;

#endif

