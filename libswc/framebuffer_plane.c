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
#include "buffer.h"
#include "drm.h"
#include "internal.h"
#include "util.h"

#include <errno.h>
#include <wld/wld.h>
#include <wld/drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

struct framebuffer
{
    uint32_t id;
    struct wl_listener destroy_listener;
};

static void handle_buffer_destroy(struct wl_listener * listener, void * data)
{
    struct framebuffer * framebuffer
        = CONTAINER_OF(listener, typeof(*framebuffer), destroy_listener);

    drmModeRmFB(swc.drm->fd, framebuffer->id);
    free(framebuffer);
}

static struct framebuffer * framebuffer_get(struct swc_buffer * buffer)
{
    struct wl_listener * listener
        = wl_signal_get(&buffer->destroy_signal, &handle_buffer_destroy);
    struct framebuffer * framebuffer;

    if (listener)
    {
        framebuffer = CONTAINER_OF(listener, typeof(*framebuffer),
                                   destroy_listener);
    }
    else
    {
        struct wld_buffer * wld = buffer->wld;
        union wld_object object;

        if (!wld_export(wld, WLD_DRM_OBJECT_HANDLE, &object))
        {
            ERROR("Could not get buffer handle\n");
            goto error0;
        }


        if (!(framebuffer = malloc(sizeof *framebuffer)))
            goto error0;

        if (drmModeAddFB(swc.drm->fd, wld->width, wld->height, 24, 32,
                         wld->pitch, object.u32, &framebuffer->id) != 0)
        {
            goto error1;
        }

        framebuffer->destroy_listener.notify = &handle_buffer_destroy;
        wl_signal_add(&buffer->destroy_signal, &framebuffer->destroy_listener);
    }

    return framebuffer;

  error1:
    free(framebuffer);
  error0:
    return NULL;
}

static bool update(struct swc_view * view)
{
    return true;
}

static bool attach(struct swc_view * view, struct swc_buffer * buffer)
{
    struct swc_framebuffer_plane * plane
        = CONTAINER_OF(view, typeof(*plane), view);
    struct framebuffer * framebuffer = framebuffer_get(buffer);

    if (!framebuffer)
        return false;

    if (drmModePageFlip(swc.drm->fd, plane->crtc, framebuffer->id,
                        DRM_MODE_PAGE_FLIP_EVENT, &plane->drm_handler) != 0)
    {
        if (errno == EINVAL)
        {
            WARNING("Page flip failed with EINVAL, trying to set CRTC\n");

            if (drmModeSetCrtc(swc.drm->fd, plane->crtc, framebuffer->id, 0, 0,
                               plane->connectors.data,
                               plane->connectors.size / 4,
                               &plane->mode.info) == 0)
            {
                swc_view_frame(&plane->view, swc_time());
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
            ERROR("Could not schedule page flip: %s\n", strerror(errno));
            return false;
        }
    }

    return true;
}

const static struct swc_view_impl view_impl = {
    .update = &update,
    .attach = &attach
};

static void handle_page_flip(struct swc_drm_handler * handler, uint32_t time)
{
    struct swc_framebuffer_plane * plane
        = CONTAINER_OF(handler, typeof(*plane), drm_handler);

    swc_view_frame(&plane->view, time);
}

bool swc_framebuffer_plane_initialize(struct swc_framebuffer_plane * plane,
                                      uint32_t crtc, drmModeModeInfoPtr mode,
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
                       connectors, num_connectors, mode) != 0)
    {
        ERROR("Failed to set CRTC: %s\n", strerror(errno));
        goto error2;
    }

    plane->crtc = crtc;
    plane->drm_handler.page_flip = &handle_page_flip;
    swc_view_initialize(&plane->view, &view_impl);
    swc_mode_initialize(&plane->mode, mode);

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
    drmModeCrtcPtr crtc = plane->original_crtc_state;
    drmModeSetCrtc(swc.drm->fd, crtc->crtc_id, crtc->buffer_id,
                   crtc->x, crtc->y, NULL, 0, &crtc->mode);
    drmModeFreeCrtc(crtc);
}

