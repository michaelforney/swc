/* swc: cursor_surface.c
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

#include "cursor_surface.h"
#include "compositor.h"
#include "output.h"
#include "plane.h"

#include <string.h>
#include <wld/wld.h>

/* Cursor view */
static const uint32_t cursor_buffer_size = 64 * 64 * 4;

static void update_plane(struct swc_plane * plane, void * data)
{
    struct wld_drawable * drawable = swc_plane_get_buffer(plane);

    wld_write(drawable, data, cursor_buffer_size);
    swc_plane_flip(plane);
}

static void attach(struct swc_surface * surface,
                   struct wl_resource * resource)
{
    struct swc_compositor * compositor = CONTAINER_OF
        (surface->view, typeof(*compositor), cursor_view);

    if (pixman_region32_not_empty(&surface->state.damage))
    {
        struct wl_shm_buffer * buffer = wl_shm_buffer_get(resource);
        uint32_t width, height;

        if (!buffer)
            return;

        width = wl_shm_buffer_get_width(buffer);
        height = wl_shm_buffer_get_height(buffer);

        if (width <= 64 && height <= 64)
        {
            struct swc_output * output;
            char data[cursor_buffer_size];

            memset(data, 0, sizeof data);
            pixman_blt(wl_shm_buffer_get_data(buffer), (uint32_t *) data,
                       wl_shm_buffer_get_stride(buffer) >> 2, 64, 32, 32,
                       0, 0, 0, 0, width, height);
            wl_buffer_send_release(resource);
            pixman_region32_clear(&surface->state.damage);

            wl_list_for_each(output, &compositor->outputs, link)
            {
                if (swc_rectangle_overlap(&output->geometry,
                                          &surface->geometry))
                {
                    update_plane(&output->cursor_plane, data);
                }
            }
        }
    }
}

static void update(struct swc_surface * surface)
{
    swc_surface_send_frame_callbacks(surface, swc_time());
}

static void move(struct swc_surface * surface, int32_t x, int32_t y)
{
    struct swc_compositor * compositor = CONTAINER_OF
        (surface->view, typeof(*compositor), cursor_view);
    struct swc_output * output;

    surface->geometry.x = x;
    surface->geometry.y = y;

    wl_list_for_each(output, &compositor->outputs, link)
    {
        if (swc_rectangle_overlap(&output->geometry, &surface->geometry))
        {
            swc_plane_move(&output->cursor_plane,
                           surface->geometry.x - output->geometry.x,
                           surface->geometry.y - output->geometry.y);
        }
    }
}

const struct swc_view_impl swc_cursor_view_impl = {
    .attach = &attach,
    .update = &update,
    .move = &move
};

