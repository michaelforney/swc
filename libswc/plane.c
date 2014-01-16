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
#include "internal.h"
#include "mode.h"
#include "output.h"

#include <stdlib.h>
#include <stdio.h>
#include <libdrm/drm.h>
#include <wld/wld.h>
#include <wld/drm.h>

struct framebuffer
{
    struct wld_buffer * buffer;
    uint32_t id;
};

static bool framebuffer_initialize(struct swc_plane * plane)
{
    struct framebuffer * fb = swc_double_buffer_front(&plane->double_buffer);

    return drmModeSetCrtc(swc.drm->fd, plane->output->crtc_id,
                          fb->id, 0, 0, &plane->output->connector_id, 1,
                          &plane->output->current_mode->info) == 0;
}

static void * framebuffer_create_buffer(struct swc_plane * plane)
{
    struct wld_buffer * buffer;
    struct swc_output * output = plane->output;
    struct framebuffer * fb;
    union wld_object object;

    if (!(fb = malloc(sizeof *fb)))
        goto error0;

    buffer = wld_create_buffer(swc.drm->context,
                               output->geometry.width, output->geometry.height,
                               WLD_FORMAT_XRGB8888);

    if (!buffer)
    {
        fprintf(stderr, "Could not create DRM buffer for framebuffer\n");
        goto error1;
    }

    wld_export(buffer, WLD_DRM_OBJECT_HANDLE, &object);

    if (drmModeAddFB(swc.drm->fd, buffer->width, buffer->height,
                     24, 32, buffer->pitch, object.u32, &fb->id) != 0)
    {
        fprintf(stderr, "drmModeAddFB failed\n");
        goto error2;
    }

    fb->buffer = buffer;

    return buffer;

  error2:
    wld_destroy_buffer(buffer);
  error1:
    free(buffer);
  error0:
    return NULL;
}

static void framebuffer_destroy_buffer(struct swc_plane * plane, void * data)
{
    struct framebuffer * fb = data;

    drmModeRmFB(swc.drm->fd, fb->id);
    wld_destroy_buffer(fb->buffer);
}

static struct wld_buffer * framebuffer_get_buffer(void * data)
{
    struct framebuffer * fb = data;

    return fb->buffer;
}

static bool framebuffer_flip(struct swc_plane * plane)
{
    struct swc_output * output = plane->output;
    struct framebuffer * fb = swc_double_buffer_back(&plane->double_buffer);

    return drmModePageFlip(swc.drm->fd, output->crtc_id, fb->id,
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
    return wld_create_buffer(swc.drm->context, 64, 64, WLD_FORMAT_ARGB8888);
}

static void cursor_destroy_buffer(struct swc_plane * plane, void * data)
{
    struct wld_buffer * buffer = data;

    wld_destroy_buffer(buffer);
}

static struct wld_buffer * cursor_get_buffer(void * data)
{
    return data;
}

static bool cursor_flip(struct swc_plane * plane)
{
    struct wld_buffer * buffer = swc_double_buffer_back(&plane->double_buffer);
    union wld_object object;

    wld_export(buffer, WLD_DRM_OBJECT_HANDLE, &object);
    return drmModeSetCursor(swc.drm->fd, plane->output->crtc_id,
                            object.u32, 64, 64) == 0;
}

static bool cursor_move(struct swc_plane * plane, int32_t x, int32_t y)
{
    return drmModeMoveCursor(swc.drm->fd, plane->output->crtc_id, x, y) == 0;
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

struct wld_buffer * swc_plane_get_buffer(struct swc_plane * plane)
{
    void * back = swc_double_buffer_back(&plane->double_buffer);

    return plane->interface->get_buffer(back);
}

