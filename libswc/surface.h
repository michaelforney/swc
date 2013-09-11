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
#define SWC_SURFACE_H 1

#include <stdbool.h>
#include <wayland-server.h>
#include <pixman.h>

struct swc_surface;

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
    struct wl_resource * buffer;

    /* The region that needs to be repainted */
    pixman_region32_t damage;

    /* ? */
    pixman_region32_t opaque;

    /* ? */
    pixman_region32_t input;

    struct wl_list frame_callbacks;
    struct wl_listener buffer_destroy_listener;
};

/**
 * A surface class is a set of operations that can be performed on a surface.
 * This gives the compositor the ability to classify surfaces, treating some
 * specially (for example, a cursor surface).
 */
struct swc_surface_class_interface
{
    /* Called when a surface is added to the class. */
    bool (* add)(struct swc_surface * surface);

    /* Called when a surface is removed from the class. */
    void (* remove)(struct swc_surface * surface);

    /* Called when a new buffer is attached to a surface. */
    void (* attach)(struct swc_surface * surface,
                    struct wl_resource * resource);

    /* Called after a surface requests a commit. */
    void (* update)(struct swc_surface * surface);

    /* Moves the surface to the specified coordinates. */
    void (* move)(struct swc_surface * surface, int32_t x, int32_t y);
};

struct swc_surface_class
{
    const struct swc_surface_class_interface * interface;
};

struct swc_surface
{
    struct wl_resource * resource;

    struct swc_surface_state state;

    /* For usage by a shell implementation. */
    void * shell_data;

    /* Shells may want to use this destructor, guaranteed to run after all the
     * other destroy listeners, rather than a destroy listener if they want to
     * do something like change keyboard focus after the currently focused
     * surface is destroyed. */
    void (* shell_destructor)(struct swc_surface * surface);

    struct
    {
        struct swc_surface_state state;
        uint32_t commit;
        int32_t x, y;
    } pending;

    const struct swc_surface_class * class;
    void * class_state;

    uint32_t outputs;
    pixman_rectangle32_t geometry;

    struct wl_signal event_signal;
    struct wl_list link;
};

struct swc_surface * swc_surface_new(struct wl_client * client, uint32_t id);

void swc_surface_send_frame_callbacks(struct swc_surface * surface,
                                      uint32_t time);
void swc_surface_set_class(struct swc_surface * surface,
                           const struct swc_surface_class * class);

void swc_surface_update(struct swc_surface * surface);

void swc_surface_move(struct swc_surface * surface, int32_t x, int32_t y);

#endif

