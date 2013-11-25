/* swc: plane.c
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

#include "plane.h"
#include "drm.h"
#include "mode.h"
#include "output.h"

#include <stdlib.h>
#include <stdio.h>
#include <libdrm/drm.h>
#include <wld/wld.h>
#include <wld/drm.h>

struct framebuffer
{
    struct wld_drawable * drawable;
    uint32_t fb_id;
};

static bool framebuffer_initialize(struct swc_plane * plane)
{
    struct framebuffer * buffer
        = swc_double_buffer_front(&plane->double_buffer);

    return drmModeSetCrtc(plane->output->drm->fd, plane->output->crtc_id,
                          buffer->fb_id, 0, 0, &plane->output->connector_id, 1,
                          &plane->output->current_mode->info) == 0;
}

static void * framebuffer_create_buffer(struct swc_plane * plane)
{
    struct wld_drawable * drawable;
    struct swc_output * output = plane->output;
    struct framebuffer * buffer;
    uint32_t handle;

    if (!(buffer = malloc(sizeof *buffer)))
        goto error0;

    drawable = wld_drm_create_drawable(output->drm->context,
                                       output->geometry.width,
                                       output->geometry.height,
                                       WLD_FORMAT_XRGB8888);

    if (!drawable)
    {
        fprintf(stderr, "Could not create DRM drawable for framebuffer\n");
        goto error1;
    }

    handle = wld_drm_get_handle(drawable);

    if (drmModeAddFB(plane->output->drm->fd, drawable->width, drawable->height,
                     24, 32, drawable->pitch, handle, &buffer->fb_id) != 0)
    {
        fprintf(stderr, "drmModeAddFB failed\n");
        goto error2;
    }

    buffer->drawable = drawable;

    return buffer;

  error2:
    wld_destroy_drawable(drawable);
  error1:
    free(buffer);
  error0:
    return NULL;
}

static void framebuffer_destroy_buffer(struct swc_plane * plane, void * data)
{
    struct framebuffer * buffer = data;

    drmModeRmFB(plane->output->drm->fd, buffer->fb_id);
    wld_destroy_drawable(buffer->drawable);
}

static struct wld_drawable * framebuffer_get_buffer(void * data)
{
    struct framebuffer * buffer = data;

    return buffer->drawable;
}

static bool framebuffer_flip(struct swc_plane * plane)
{
    struct swc_output * output = plane->output;
    struct framebuffer * buffer = swc_double_buffer_back(&plane->double_buffer);

    return drmModePageFlip(output->drm->fd, output->crtc_id, buffer->fb_id,
                           DRM_MODE_PAGE_FLIP_EVENT, output) == 0;
}

const struct swc_plane_interface swc_framebuffer_plane = {
    .initialize = &framebuffer_initialize,
    .create_buffer = &framebuffer_create_buffer,
    .destroy_buffer = &framebuffer_destroy_buffer,
    .get_buffer = &framebuffer_get_buffer,
    .flip = &framebuffer_flip
};

static bool cursor_initialize(struct swc_plane * plane)
{
    return true;
}

static void * cursor_create_buffer(struct swc_plane * plane)
{
    return wld_drm_create_drawable(plane->output->drm->context, 64, 64,
                                   WLD_FORMAT_ARGB8888);
}

static void cursor_destroy_buffer(struct swc_plane * plane, void * data)
{
    struct wld_drawable * drawable = data;

    wld_destroy_drawable(drawable);
}

static struct wld_drawable * cursor_get_buffer(void * data)
{
    return data;
}

static bool cursor_flip(struct swc_plane * plane)
{
    struct wld_drawable * drawable
        = swc_double_buffer_back(&plane->double_buffer);
    int handle = wld_drm_get_handle(drawable);

    return drmModeSetCursor(plane->output->drm->fd, plane->output->crtc_id,
                            handle, 64, 64) == 0;
}

static bool cursor_move(struct swc_plane * plane, int32_t x, int32_t y)
{
    return drmModeMoveCursor(plane->output->drm->fd, plane->output->crtc_id,
                             x, y) == 0;
}

const struct swc_plane_interface swc_cursor_plane = {
    .initialize = &cursor_initialize,
    .create_buffer = &cursor_create_buffer,
    .destroy_buffer = &cursor_destroy_buffer,
    .get_buffer = &cursor_get_buffer,
    .flip = &cursor_flip,
    .move = &cursor_move
};

bool swc_plane_initialize(struct swc_plane * plane,
                          const struct swc_plane_interface * interface,
                          struct swc_output * output)
{
    plane->interface = interface;
    plane->output = output;
    plane->double_buffer.buffers[0] = interface->create_buffer(plane);
    plane->double_buffer.buffers[1] = interface->create_buffer(plane);
    plane->double_buffer.front = 0;
    plane->x = 0;
    plane->y = 0;

    return plane->interface->initialize(plane);
}

void swc_plane_finish(struct swc_plane * plane)
{
    plane->interface->destroy_buffer(plane, plane->double_buffer.buffers[0]);
    plane->interface->destroy_buffer(plane, plane->double_buffer.buffers[1]);
}

bool swc_plane_flip(struct swc_plane * plane)
{
    if (plane->interface->flip(plane))
    {
        swc_double_buffer_swap(&plane->double_buffer);

        return true;
    }
    else
        return false;
}

bool swc_plane_move(struct swc_plane * plane, int32_t x, int32_t y)
{
    if (plane->interface->move(plane, x, y))
    {
        plane->x = x;
        plane->y = y;

        return true;
    }
    else
        return false;
}

struct wld_drawable * swc_plane_get_buffer(struct swc_plane * plane)
{
    void * back = swc_double_buffer_back(&plane->double_buffer);

    return plane->interface->get_buffer(back);
}

