/* swc: framebuffer_plane.c
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

#include "framebuffer_plane.h"
#include "drm.h"
#include "internal.h"
#include "util.h"

#include <errno.h>
#include <wld/wld.h>
#include <wld/drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

enum
{
    WLD_USER_OBJECT_FRAMEBUFFER = WLD_USER_ID
};

struct framebuffer
{
    struct wld_exporter exporter;
    struct wld_destructor destructor;
    uint32_t id;
};

static bool framebuffer_export(struct wld_exporter * exporter,
                               struct wld_buffer * buffer,
                               uint32_t type, union wld_object * object)
{
    struct framebuffer * framebuffer
        = CONTAINER_OF(exporter, typeof(*framebuffer), exporter);

    switch (type)
    {
        case WLD_USER_OBJECT_FRAMEBUFFER:
            object->u32 = framebuffer->id;
            break;
        default: return false;
    }

    return true;
}

static void framebuffer_destroy(struct wld_destructor * destructor)
{
    struct framebuffer * framebuffer
        = CONTAINER_OF(destructor, typeof(*framebuffer), destructor);

    drmModeRmFB(swc.drm->fd, framebuffer->id);
    free(framebuffer);
}

static bool update(struct swc_view * view)
{
    return true;
}

static void send_frame(void * data)
{
    struct swc_framebuffer_plane * plane = data;

    swc_view_frame(&plane->view, swc_time());
}

static bool attach(struct swc_view * view, struct wld_buffer * buffer)
{
    struct swc_framebuffer_plane * plane
        = CONTAINER_OF(view, typeof(*plane), view);
    union wld_object object;

    if (!wld_export(buffer, WLD_USER_OBJECT_FRAMEBUFFER, &object))
    {
        struct framebuffer * framebuffer;

        if (!wld_export(buffer, WLD_DRM_OBJECT_HANDLE, &object))
        {
            ERROR("Could not get buffer handle\n");
            return false;
        }

        if (!(framebuffer = malloc(sizeof *framebuffer)))
            return false;

        if (drmModeAddFB(swc.drm->fd, buffer->width, buffer->height, 24, 32,
                         buffer->pitch, object.u32, &framebuffer->id) != 0)
        {
            free(framebuffer);
            return false;
        }

        framebuffer->exporter.export = &framebuffer_export;
        wld_buffer_add_exporter(buffer, &framebuffer->exporter);
        framebuffer->destructor.destroy = &framebuffer_destroy;
        wld_buffer_add_destructor(buffer, &framebuffer->destructor);

        object.u32 = framebuffer->id;
    }

    if (plane->need_modeset)
    {
        if (drmModeSetCrtc(swc.drm->fd, plane->crtc, object.u32, 0, 0,
                           plane->connectors.data, plane->connectors.size / 4,
                           &plane->mode.info) == 0)
        {
            wl_event_loop_add_idle(swc.event_loop, &send_frame, plane);
            plane->need_modeset = false;
        }
        else
        {
            ERROR("Could not set CRTC to next framebuffer: %s\n",
                  strerror(errno));
            return false;
        }
    }
    else
    {
        if (drmModePageFlip(swc.drm->fd, plane->crtc, object.u32,
                            DRM_MODE_PAGE_FLIP_EVENT, &plane->drm_handler) != 0)
        {
            ERROR("Page flip failed: %s\n", strerror(errno));
            return false;
        }
    }

    return true;
}

static bool move(struct swc_view * view, int32_t x, int32_t y)
{
    swc_view_set_position(view, x, y);

    return true;
}

const static struct swc_view_impl view_impl = {
    .update = &update,
    .attach = &attach,
    .move = &move
};

static void handle_page_flip(struct swc_drm_handler * handler, uint32_t time)
{
    struct swc_framebuffer_plane * plane
        = CONTAINER_OF(handler, typeof(*plane), drm_handler);

    swc_view_frame(&plane->view, time);
}

bool swc_framebuffer_plane_initialize(struct swc_framebuffer_plane * plane,
                                      uint32_t crtc, struct swc_mode * mode,
                                      uint32_t * connectors,
                                      uint32_t num_connectors)
{
    uint32_t * plane_connectors;

    if (!(plane->original_crtc_state = drmModeGetCrtc(swc.drm->fd, crtc)))
    {
        ERROR("Failed to get CRTC state for CRTC %u: %s\n",
              crtc, strerror(errno));
        goto error0;
    }

    wl_array_init(&plane->connectors);
    plane_connectors = wl_array_add(&plane->connectors,
                                    num_connectors * sizeof connectors[0]);

    if (!plane_connectors)
    {
        ERROR("Failed to allocate connector array\n");
        goto error1;
    }

    memcpy(plane_connectors, connectors, num_connectors * sizeof connectors[0]);

    if (drmModeSetCrtc(swc.drm->fd, crtc, -1, 0, 0,
                       connectors, num_connectors, &mode->info) != 0)
    {
        ERROR("Failed to set CRTC: %s\n", strerror(errno));
        goto error2;
    }

    plane->crtc = crtc;
    plane->drm_handler.page_flip = &handle_page_flip;
    plane->need_modeset = true;
    swc_view_initialize(&plane->view, &view_impl);
    plane->view.geometry.width = mode->width;
    plane->view.geometry.height = mode->height;
    plane->mode = *mode;

    return true;

  error2:
    wl_array_release(&plane->connectors);
  error1:
    drmModeFreeCrtc(plane->original_crtc_state);
  error0:
    return false;
}

void swc_framebuffer_plane_finalize(struct swc_framebuffer_plane * plane)
{
    wl_array_release(&plane->connectors);
    drmModeCrtcPtr crtc = plane->original_crtc_state;
    drmModeSetCrtc(swc.drm->fd, crtc->crtc_id, crtc->buffer_id,
                   crtc->x, crtc->y, NULL, 0, &crtc->mode);
    drmModeFreeCrtc(crtc);
}

