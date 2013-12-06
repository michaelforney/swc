/* swc: libswc/swc.c
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

#include "swc.h"
#include "bindings.h"
#include "compositor.h"
#include "data_device_manager.h"
#include "internal.h"
#include "keyboard.h"
#include "pointer.h"
#include "seat.h"
#include "shell.h"
#include "window.h"
#ifdef ENABLE_XWAYLAND
# include "xserver.h"
#endif

#include <libudev.h>

extern const struct swc_seat_global seat_global;
extern const struct swc_bindings_global bindings_global;
static struct swc_compositor compositor;

struct swc swc = {
    .seat = &seat_global,
    .bindings = &bindings_global,
    .compositor = &compositor
};

static void setup_compositor()
{
    pixman_region32_t pointer_region;
    struct swc_output * output;

    wl_list_insert(&swc.seat->keyboard->handlers,
                   &swc.bindings->keyboard_handler->link);
    swc.seat->pointer->handler = &compositor.pointer_handler;
    wl_signal_add(&swc.seat->pointer->focus.event_signal,
                  swc_window_enter_listener);
    wl_signal_add(&swc.seat->pointer->event_signal,
                  &compositor.pointer_listener);

    /* Calculate pointer region */
    pixman_region32_init(&pointer_region);

    wl_list_for_each(output, &compositor.outputs, link)
    {
        pixman_region32_union_rect(&pointer_region, &pointer_region,
                                   output->geometry.x, output->geometry.y,
                                   output->geometry.width,
                                   output->geometry.height);
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

    if (!(swc.udev = udev_new()))
    {
        fprintf(stderr, "Could not initialize udev\n");
        goto error0;
    }

    if (!swc_compositor_initialize(&compositor, display, swc.event_loop))
    {
        fprintf(stderr, "Could not initialize compositor\n");
        goto error1;
    }

    swc_compositor_add_globals(&compositor, display);

    if (!swc_data_device_manager_initialize())
    {
        ERROR("Could not initialize data device manager\n");
        goto error2;
    }

    if (!swc_seat_initialize())
    {
        fprintf(stderr, "Could not initialize seat\n");
        goto error3;
    }

    if (!swc_bindings_initialize())
    {
        fprintf(stderr, "Could not initialize bindings\n");
        goto error4;
    }

    if (!swc_shell_initialize())
    {
        fprintf(stderr, "Could not initialize shell\n");
        goto error5;
    }

#ifdef ENABLE_XWAYLAND
    if (!swc_xserver_initialize())
    {
        fprintf(stderr, "Could not initialize xwayland\n");
        goto error6;
    }
#endif

    setup_compositor();

    return true;

  error6:
    swc_shell_finalize();
  error5:
    swc_bindings_finalize();
  error4:
    swc_seat_finalize();
  error3:
    swc_data_device_manager_finalize();
  error2:
    swc_compositor_finish(&compositor);
  error1:
    udev_unref(swc.udev);
  error0:
    return false;
}

EXPORT
void swc_finalize()
{
#ifdef ENABLE_XWAYLAND
    swc_xserver_finalize();
#endif
    swc_shell_finalize();
    swc_bindings_finalize();
    swc_seat_finalize();
    swc_data_device_manager_finalize();
    swc_compositor_finish(&compositor);
    udev_unref(swc.udev);
}

