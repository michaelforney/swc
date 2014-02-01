/* swc: libswc/swc.c
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

#include "swc.h"
#include "bindings.h"
#include "compositor.h"
#include "data_device_manager.h"
#include "drm.h"
#include "internal.h"
#include "launch.h"
#include "keyboard.h"
#include "panel_manager.h"
#include "pointer.h"
#include "screen.h"
#include "seat.h"
#include "shell.h"
#include "shm.h"
#include "util.h"
#include "window.h"
#ifdef ENABLE_XWAYLAND
# include "xserver.h"
#endif

extern struct swc_launch swc_launch;
extern const struct swc_seat swc_seat;
extern const struct swc_bindings swc_bindings;
extern const struct swc_compositor swc_compositor;
extern struct swc_drm swc_drm;
extern struct swc_shm swc_shm;

struct swc swc = {
    .launch = &swc_launch,
    .seat = &swc_seat,
    .bindings = &swc_bindings,
    .compositor = &swc_compositor,
    .drm = &swc_drm,
    .shm = &swc_shm
};

static void setup_compositor()
{
    pixman_region32_t pointer_region;
    struct screen * screen;
    struct swc_rectangle * geometry;

    wl_list_insert(&swc.seat->keyboard->handlers,
                   &swc.bindings->keyboard_handler->link);
    swc.seat->pointer->handler = swc.compositor->pointer_handler;
    wl_signal_add(&swc.seat->pointer->focus.event_signal,
                  &window_enter_listener);

    /* Calculate pointer region */
    pixman_region32_init(&pointer_region);

    wl_list_for_each(screen, &swc.screens, link)
    {
        geometry = &screen->base.geometry;
        pixman_region32_union_rect(&pointer_region, &pointer_region,
                                   geometry->x, geometry->y,
                                   geometry->width, geometry->height);
    }

    swc_pointer_set_region(swc.seat->pointer, &pointer_region);
    pixman_region32_fini(&pointer_region);
}

EXPORT
bool swc_initialize(struct wl_display * display,
                    struct wl_event_loop * event_loop,
                    const struct swc_manager * manager)
{
    swc.display = display;
    swc.event_loop = event_loop ?: wl_display_get_event_loop(display);
    swc.manager = manager;
    const char * default_seat = "seat0";

    if (!(swc_launch_initialize()))
    {
        ERROR("Could not connect to swc-launch\n");
        goto error0;
    }

    if (!swc_drm_initialize())
    {
        ERROR("Could not initialize DRM\n");
        goto error1;
    }

    if (!swc_shm_initialize())
    {
        ERROR("Could not initialize SHM\n");
        goto error2;
    }

    if (!swc_bindings_initialize())
    {
        ERROR("Could not initialize bindings\n");
        goto error3;
    }

    if (!screens_initialize())
    {
        ERROR("Could not initialize screens\n");
        goto error4;
    }

    if (!swc_compositor_initialize())
    {
        ERROR("Could not initialize compositor\n");
        goto error5;
    }

    if (!swc_data_device_manager_initialize())
    {
        ERROR("Could not initialize data device manager\n");
        goto error6;
    }

    if (!swc_seat_initialize(default_seat))
    {
        ERROR("Could not initialize seat\n");
        goto error7;
    }

    if (!swc_shell_initialize())
    {
        ERROR("Could not initialize shell\n");
        goto error8;
    }

    if (!swc_panel_manager_initialize())
    {
        ERROR("Could not initialize panel manager\n");
        goto error9;
    }

#ifdef ENABLE_XWAYLAND
    if (!swc_xserver_initialize())
    {
        ERROR("Could not initialize xwayland\n");
        goto error10;
    }
#endif

    setup_compositor();

    return true;

#ifdef ENABLE_XWAYLAND
  error10:
    swc_panel_manager_finalize();
#endif
  error9:
    swc_shell_finalize();
  error8:
    swc_seat_finalize();
  error7:
    swc_data_device_manager_finalize();
  error6:
    swc_compositor_finalize();
  error5:
    screens_finalize();
  error4:
    swc_bindings_finalize();
  error3:
    swc_shm_finalize();
  error2:
    swc_drm_finalize();
  error1:
    swc_launch_finalize();
  error0:
    return false;
}

EXPORT
void swc_finalize()
{
#ifdef ENABLE_XWAYLAND
    swc_xserver_finalize();
#endif
    swc_panel_manager_finalize();
    swc_shell_finalize();
    swc_seat_finalize();
    swc_data_device_manager_finalize();
    swc_compositor_finalize();
    screens_finalize();
    swc_bindings_finalize();
    swc_shm_finalize();
    swc_drm_finalize();
    swc_launch_finalize();
}

