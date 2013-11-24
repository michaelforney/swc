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
#include "private.h"
#include "binding.h"
#include "compositor.h"
#include "shell.h"
#include "window.h"

static struct swc_compositor compositor;

struct swc swc = {
    .compositor = &compositor
};

static void setup_compositor()
{
    compositor.seat.keyboard.handler = swc_binding_handler;
    wl_signal_add(&compositor.seat.pointer.focus.event_signal,
                  swc_window_enter_listener);
}

EXPORT
bool swc_initialize(struct wl_display * display,
                    struct wl_event_loop * event_loop,
                    const struct swc_manager * manager)
{
    swc.display = display;
    swc.event_loop = event_loop ?: wl_display_get_event_loop(display);
    swc.manager = manager;

    if (!swc_bindings_initialize())
    {
        fprintf(stderr, "Could not initialize bindings\n");
        goto error0;
    }

    if (!swc_compositor_initialize(&compositor, display, swc.event_loop))
    {
        fprintf(stderr, "Could not initialize compositor\n");
        goto error1;
    }

    swc_compositor_add_globals(&compositor, display);

    if (!swc_shell_initialize(display))
    {
        fprintf(stderr, "Could not initialize shell\n");
        goto error2;
    }

    setup_compositor();

    return true;

  error2:
    swc_compositor_finish(&compositor);
  error1:
    swc_bindings_finalize();
  error0:
    return false;
}

EXPORT
void swc_finalize()
{
    swc_shell_finalize();
    swc_compositor_finish(&compositor);
    swc_bindings_finalize();
}

