/* swc: tty.c
 *
 * Copyright © 2012 Michael Forney
 *
 * Based in part upon tty.c from weston, which is:
 *
 *      Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <linux/kd.h>

#include <wayland-server.h>

#include "tty.h"
#include "event.h"

static void restore_tty(struct swc_tty * tty);
static int handle_vt_signal(int signal_number, void * data);

bool swc_tty_initialize(struct swc_tty * tty,
                        struct wl_event_loop * event_loop,
                        uint8_t tty_number)
{
    struct vt_stat state;
    char tty_device[16];
    struct vt_mode mode;

    wl_signal_init(&tty->event_signal);

    if (tty_number == 0)
    {
        char * vt_string = getenv("XDG_VTNR");
        if (vt_string)
        {
            char * end;
            tty_number = strtoul(vt_string, &end, 10);
            if (*end != '\0')
                tty_number = 0;
        }
    }

    /* If we still don't have a VT number. */
    if (tty_number == 0)
    {
        printf("don't know which VT to run on\n");
        goto error_base;
    }

    snprintf(tty_device, sizeof(tty_device), "/dev/tty%u", tty_number);

    /* Open the TTY. */
    tty->fd = open(tty_device, O_RDWR | O_NOCTTY | O_CLOEXEC);

    if (tty->fd == -1)
    {
        printf("couldn't open tty\n");
        goto error_base;
    }

    tty->vt = tty_number;

    /* Determine the current VT state. */
    if (ioctl(tty->fd, VT_GETSTATE, &state) != 0)
    {
        printf("could not determine starting vt\n");
        goto error_tty;
    }

    tty->original_state.vt = state.v_active;
    printf("starting vt: %u\n", tty->original_state.vt);

    /* Switch to the new VT if necessary. */
    if (tty->original_state.vt != tty->vt)
    {
        if (ioctl(tty->fd, VT_ACTIVATE, tty->vt) != 0
            || ioctl(tty->fd, VT_WAITACTIVE, tty->vt) != 0)
        {
            printf("couldn't switch to vt%u\n", tty->vt);
            goto error_tty;
        }
    }

    tty->active = true;

    /* Save current kb_mode. */
    if (ioctl(tty->fd, KDGKBMODE, &tty->original_state.kb_mode) != 0)
    {
        printf("couldn't determine kb_mode of vt%u\n", tty->vt);
        goto error_tty;
    }

    /* Turn off keyboard, we will use evdev for input. */
    if (ioctl(tty->fd, KDSKBMODE, K_OFF) != 0)
    {
        printf("couldn't set kb_mode of vt%u to K_OFF\n", tty->vt);
        goto error_tty;
    }

    /* Set VT to graphics mode. */
    if (ioctl(tty->fd, KDSETMODE, KD_GRAPHICS) != 0)
    {
        printf("couldn't set mode of vt%u to KD_GRAPHICS\n", tty->vt);
        goto error_kdkbmode;
    }

    mode = (struct vt_mode) {
        .mode = VT_PROCESS,
        .relsig = SIGUSR1,
        .acqsig = SIGUSR1
    };

    /* Set up VT switching handler. */
    if (ioctl(tty->fd, VT_SETMODE, &mode) != 0)
    {
        printf("could not set VT mode on vt%u\n", tty->vt);
        goto error_kdmode;
    }

    tty->vt_source = wl_event_loop_add_signal(event_loop, SIGUSR1,
                                              &handle_vt_signal, tty);

    if (!tty->vt_source)
    {
        printf("could not create VT event source\n");
        goto error_vtmode;
    }

    return true;

  error_vtmode:
    mode = (struct vt_mode) { .mode = VT_AUTO };
    ioctl(tty->fd, VT_SETMODE, &mode);
  error_kdmode:
    ioctl(tty->fd, KDSETMODE, KD_TEXT);
  error_kdkbmode:
    ioctl(tty->fd, KDSKBMODE, tty->original_state.kb_mode);
  error_tty:
    close(tty->fd);
  error_base:
    return false;
}

void swc_tty_finish(struct swc_tty * tty)
{
    wl_event_source_remove(tty->vt_source);
    restore_tty(tty);
    close(tty->fd);
}

void swc_tty_switch_vt(struct swc_tty * tty, uint32_t vt)
{
    ioctl(tty->fd, VT_ACTIVATE, vt);
}

void restore_tty(struct swc_tty * tty)
{
    struct vt_mode mode = { .mode = VT_AUTO };

    if (ioctl(tty->fd, KDSKBMODE, tty->original_state.kb_mode) != 0)
        printf("failed to restore keyboard mode\n");
    if (ioctl(tty->fd, KDSETMODE, KD_TEXT) != 0)
        printf("failed to set mode to KD_TEXT\n");
    if (ioctl(tty->fd, VT_SETMODE, &mode) != 0)
        printf("failed to restore VT handling\n");
    if (tty->vt != tty->original_state.vt
        && (ioctl(tty->fd, VT_ACTIVATE, tty->original_state.vt) != 0
            || ioctl(tty->fd, VT_WAITACTIVE, tty->original_state.vt) != 0))
    {
        printf("failed to restore VT\n");
    }
}

static int handle_vt_signal(int signal_number, void * data)
{
    struct swc_tty * tty = data;

    if (tty->active)
    {
        swc_send_event(&tty->event_signal, SWC_TTY_VT_LEAVE, NULL);
        ioctl(tty->fd, VT_RELDISP, 1);
        tty->active = false;
    }
    else
    {
        ioctl(tty->fd, VT_RELDISP, VT_ACKACQ);
        tty->active = true;
        swc_send_event(&tty->event_signal, SWC_TTY_VT_ENTER, NULL);
    }

    return 1;
}

