/* swc: cursor_plane.c
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

#include "cursor_plane.h"
#include "drm.h"
#include "internal.h"
#include "launch.h"
#include "screen.h"
#include "util.h"

#include <errno.h>
#include <wld/wld.h>
#include <wld/drm.h>
#include <xf86drmMode.h>

static bool update(struct view * view)
{
    return true;
}

static int attach(struct view * view, struct wld_buffer * buffer)
{
    struct cursor_plane * plane = wl_container_of(view, plane, view);
    int ret;

    if (buffer)
    {
        union wld_object object;

        if (!wld_export(buffer, WLD_DRM_OBJECT_HANDLE, &object))
        {
            ERROR("Could not get export buffer to DRM handle\n");
            /* XXX: Not the best error code, but we don't know better until wld
             * returns an actual error code. */
            return -EINVAL;
        }

        ret = drmModeSetCursor(swc.drm->fd, plane->crtc, object.u32,
                               buffer->width, buffer->height);

        if (ret < 0)
        {
            ERROR("Could not set cursor: %s\n", strerror(-ret));
            return ret;
        }
    }
    else
    {
        if ((ret = drmModeSetCursor(swc.drm->fd, plane->crtc, 0, 0, 0)) < 0)
        {
            ERROR("Could not unset cursor: %s\n", strerror(-ret));
            return ret;
        }
    }

    view_set_size_from_buffer(view, buffer);
    return 0;
}

static bool move(struct view * view, int32_t x, int32_t y)
{
    struct cursor_plane * plane = wl_container_of(view, plane, view);

    if (drmModeMoveCursor(swc.drm->fd, plane->crtc,
                          x - plane->origin->x, y - plane->origin->y) != 0)
    {
        ERROR("Could not move cursor: %s\n", strerror(errno));
        return false;
    }

    view_set_position(view, x, y);

    return true;
}

static const struct view_impl view_impl = {
    .update = &update,
    .attach = &attach,
    .move = &move
};

static void handle_launch_event(struct wl_listener * listener, void * data)
{
    struct swc_event * event = data;
    struct cursor_plane * plane
        = wl_container_of(listener, plane, launch_listener);

    switch (event->type)
    {
        case SWC_LAUNCH_EVENT_ACTIVATED:
            move(&plane->view, plane->view.geometry.x, plane->view.geometry.y);
            attach(&plane->view, plane->view.buffer);
            break;
    }
}

bool cursor_plane_initialize(struct cursor_plane * plane, uint32_t crtc,
                             const struct swc_rectangle * origin)
{
    if (drmModeSetCursor(swc.drm->fd, crtc, 0, 0, 0) != 0)
        return false;

    plane->origin = origin;
    plane->crtc = crtc;
    plane->launch_listener.notify = &handle_launch_event;
    wl_signal_add(&swc.launch->event_signal, &plane->launch_listener);
    view_initialize(&plane->view, &view_impl);

    return true;
}

void cursor_plane_finalize(struct cursor_plane * plane)
{
    drmModeSetCursor(swc.drm->fd, plane->crtc, 0, 0, 0);
}

