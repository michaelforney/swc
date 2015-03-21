/* swc: libswc/evdev_device.h
 *
 * Copyright (c) 2013 Michael Forney
 *
 * Based in part upon evdev.h from weston, which is:
 *
 *     Copyright © 2011, 2012 Intel Corporation
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

#ifndef SWC_EVDEV_DEVICE_H
#define SWC_EVDEV_DEVICE_H

#include <stdbool.h>
#include <linux/input.h>
#include <wayland-util.h>

struct evdev_device;
struct wl_event_loop;

struct evdev_device_handler
{
    void (* key)(uint32_t time, uint32_t key, uint32_t state);
    void (* button)(uint32_t time, uint32_t key, uint32_t state);
    void (* axis)(uint32_t time, uint32_t axis, wl_fixed_t amount);
    void (* relative_motion)(uint32_t time, wl_fixed_t dx, wl_fixed_t dy);
};

struct evdev_device
{
    char * path;
    int fd;
    struct libevdev * dev;
    bool needs_sync;

    const struct evdev_device_handler * handler;

    struct
    {
        struct
        {
            struct
            {
                struct input_absinfo x, y;
            } info;

            int32_t x, y;
            bool pending;
        } abs;

        struct
        {
            int32_t dx, dy;
            bool pending;
        } rel;
    } motion;

    uint32_t capabilities;

    struct wl_event_source * source;
    struct wl_list link;
};

struct evdev_device * evdev_device_new
    (const char * path, const struct evdev_device_handler * handler);

void evdev_device_destroy(struct evdev_device * device);

bool evdev_device_reopen(struct evdev_device * device);

#endif

