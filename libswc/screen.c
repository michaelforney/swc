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
#include "internal.h"
#include "mode.h"
#include "output.h"
#include "util.h"

#include <stdlib.h>
#include <sys/param.h>

#define INTERNAL(screen) ((struct swc_screen_internal *) (screen))

bool swc_screens_initialize()
{
    wl_list_init(&swc.screens);

    if (!swc_drm_create_screens(&swc.screens))
        return false;

    if (wl_list_empty(&swc.screens))
        return false;

    return true;
}

void swc_screens_finalize()
{
    struct swc_screen_internal * screen, * tmp;

    wl_list_for_each_safe(screen, tmp, &swc.screens, link)
        swc_screen_destroy(screen);
}

struct swc_screen_internal * swc_screen_new(uint32_t crtc,
                                            struct swc_output * output)
{
    struct swc_screen_internal * screen;
    int32_t x = 0;

    /* Simple heuristic for initial screen positioning. */
    wl_list_for_each(screen, &swc.screens, link)
        x = MAX(x, screen->base.geometry.x + screen->base.geometry.width);

    if (!(screen = malloc(sizeof *screen)))
        goto error0;

    screen->base.geometry.x = x;
    screen->base.geometry.y = 0;
    screen->base.geometry.width = output->preferred_mode->width;
    screen->base.geometry.height = output->preferred_mode->height;
    screen->base.usable_geometry = screen->base.geometry;
    wl_signal_init(&screen->base.event_signal);
    wl_list_init(&screen->outputs);
    wl_list_insert(&INTERNAL(screen)->outputs, &output->link);

    if (!swc_framebuffer_plane_initialize(&screen->planes.framebuffer, crtc,
                                          &output->preferred_mode->info,
                                          &output->connector, 1))
    {
        ERROR("Failed to initialize framebuffer plane\n");
        goto error1;
    }

    if (!swc_cursor_plane_initialize(&screen->planes.cursor, crtc))
    {
        ERROR("Failed to initialize cursor plane\n");
        goto error2;
    }

    swc.manager->new_screen(&screen->base);

    return screen;

  error2:
    swc_framebuffer_plane_finalize(&screen->planes.framebuffer);
  error1:
    free(screen);
  error0:
    return NULL;
}

void swc_screen_destroy(struct swc_screen_internal * screen)
{
    struct swc_output * output, * next;

    wl_list_for_each_safe(output, next, &screen->outputs, link)
        swc_output_destroy(output);
    swc_framebuffer_plane_finalize(&screen->planes.framebuffer);
    swc_cursor_plane_finalize(&screen->planes.cursor);
    free(screen);
}

