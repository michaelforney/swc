/* swc: surface.h
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

#ifndef SWC_SURFACE_H
#define SWC_SURFACE_H

#include <stdbool.h>
#include <wayland-server.h>
#include <pixman.h>

enum swc_surface_event_type
{
    SWC_SURFACE_EVENT_TYPE_RESIZE
};

struct swc_surface_event_data
{
    struct swc_surface * surface;

    struct
    {
        uint32_t old_width, old_height;
        uint32_t new_width, new_height;
    } resize;
};

enum swc_surface_commit_info
{
    SWC_SURFACE_COMMIT_ATTACH = (1 << 0),
    SWC_SURFACE_COMMIT_DAMAGE = (1 << 1),
    SWC_SURFACE_COMMIT_OPAQUE = (1 << 2),
    SWC_SURFACE_COMMIT_INPUT = (1 << 3),
    SWC_SURFACE_COMMIT_FRAME = (1 << 4)
};

struct swc_surface_state
{
    struct swc_buffer * buffer;
    struct wl_listener buffer_destroy_listener;

    /* The region that needs to be repainted. */
    pixman_region32_t damage;

    /* The region that is opaque. */
    pixman_region32_t opaque;

    /* The region that accepts input. */
    pixman_region32_t input;

    struct wl_list frame_callbacks;
};

struct swc_surface
{
    struct wl_resource * resource;

    struct swc_surface_state state;

    struct
    {
        struct swc_surface_state state;
        uint32_t commit;
        int32_t x, y;
    } pending;

    struct swc_window * window;
    struct swc_view * view;
    struct wl_listener view_listener;

    struct wl_signal event_signal;
    struct wl_list link;
};

struct swc_surface * swc_surface_new(struct wl_client * client,
                                     uint32_t version, uint32_t id);

void swc_surface_set_view(struct swc_surface * surface, struct swc_view * view);

#endif

