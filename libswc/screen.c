/* swc: libswc/screen.c
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

#include "screen.h"
#include "drm.h"
#include "event.h"
#include "internal.h"
#include "mode.h"
#include "output.h"
#include "util.h"
#include "protocol/swc-server-protocol.h"

#include <stdlib.h>
#include <sys/param.h>

#define INTERNAL(s) ((struct screen *) (s))

bool screens_initialize()
{
    wl_list_init(&swc.screens);

    if (!swc_drm_create_screens(&swc.screens))
        return false;

    if (wl_list_empty(&swc.screens))
        return false;

    return true;
}

void screens_finalize()
{
    struct screen * screen, * tmp;

    wl_list_for_each_safe(screen, tmp, &swc.screens, link)
        screen_destroy(screen);
}

static void bind_screen(struct wl_client * client, void * data,
                        uint32_t version, uint32_t id)
{
    struct screen * screen = data;
    struct wl_resource * resource;

    if (version >= 1)
        version = 1;

    resource = wl_resource_create(client, &swc_screen_interface, version, id);

    if (!resource)
    {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(resource, NULL,
                                   screen, &swc_remove_resource);
    wl_list_insert(&screen->resources, wl_resource_get_link(resource));
}

struct screen * screen_new(uint32_t crtc, struct swc_output * output)
{
    struct screen * screen;
    int32_t x = 0;

    /* Simple heuristic for initial screen positioning. */
    wl_list_for_each(screen, &swc.screens, link)
        x = MAX(x, screen->base.geometry.x + screen->base.geometry.width);

    if (!(screen = malloc(sizeof *screen)))
        goto error0;

    screen->global = wl_global_create(swc.display, &swc_screen_interface, 1,
                                      screen, &bind_screen);

    if (!screen->global)
    {
        ERROR("Failed to create screen global\n");
        goto error1;
    }

    if (!framebuffer_plane_initialize(&screen->planes.framebuffer, crtc,
                                      output->preferred_mode,
                                      &output->connector, 1))
    {
        ERROR("Failed to initialize framebuffer plane\n");
        goto error2;
    }

    if (!cursor_plane_initialize(&screen->planes.cursor, crtc,
                                 &screen->base.geometry))
    {
        ERROR("Failed to initialize cursor plane\n");
        goto error3;
    }

    wl_signal_init(&screen->base.event_signal);
    wl_list_init(&screen->resources);
    wl_list_init(&screen->outputs);
    wl_list_insert(&screen->outputs, &output->link);
    wl_list_init(&screen->modifiers);

    view_move(&screen->planes.framebuffer.view, x, 0);
    screen->base.geometry = screen->planes.framebuffer.view.geometry;
    screen->base.usable_geometry = screen->base.geometry;

    swc.manager->new_screen(&screen->base);

    return screen;

  error3:
    framebuffer_plane_finalize(&screen->planes.framebuffer);
  error2:
    wl_global_destroy(screen->global);
  error1:
    free(screen);
  error0:
    return NULL;
}

void screen_destroy(struct screen * screen)
{
    struct swc_output * output, * next;

    swc_send_event(&screen->base.event_signal, SWC_SCREEN_DESTROYED, NULL);
    wl_list_for_each_safe(output, next, &screen->outputs, link)
        swc_output_destroy(output);
    framebuffer_plane_finalize(&screen->planes.framebuffer);
    cursor_plane_finalize(&screen->planes.cursor);
    free(screen);
}

void screen_update_usable_geometry(struct screen * screen)
{
    pixman_region32_t total_usable, usable;
    pixman_box32_t * extents;
    struct screen_modifier * modifier;

    DEBUG("Updating usable geometry\n");

    pixman_region32_init_rect(&total_usable,
                              screen->base.geometry.x, screen->base.geometry.y,
                              screen->base.geometry.width,
                              screen->base.geometry.height);
    pixman_region32_init(&usable);

    wl_list_for_each(modifier, &screen->modifiers, link)
    {
        modifier->modify(modifier, &screen->base.geometry, &usable);
        pixman_region32_intersect(&total_usable, &total_usable, &usable);
    }

    extents = pixman_region32_extents(&total_usable);

    if (extents->x1 != screen->base.usable_geometry.x
        || extents->y1 != screen->base.usable_geometry.y
        || (extents->x2 - extents->x1) != screen->base.usable_geometry.width
        || (extents->y2 - extents->y1) != screen->base.usable_geometry.height)
    {
        screen->base.usable_geometry.x = extents->x1;
        screen->base.usable_geometry.y = extents->y1;
        screen->base.usable_geometry.width = extents->x2 - extents->x1;
        screen->base.usable_geometry.height = extents->y2 - extents->y1;

        swc_send_event(&screen->base.event_signal,
                       SWC_SCREEN_USABLE_GEOMETRY_CHANGED, NULL);
    }
}

