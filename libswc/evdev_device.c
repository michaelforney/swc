/* swc: libswc/evdev_device.c
 *
 * Copyright (c) 2013, 2014 Michael Forney
 *
 * Based in part upon evdev.c from weston, which is:
 *
 *     Copyright © 2010 Intel Corporation
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

#include "evdev_device.h"
#include "event.h"
#include "internal.h"
#include "launch.h"
#include "seat.h"
#include "util.h"

#include <errno.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define AXIS_STEP_DISTANCE 10
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

static inline uint32_t
timeval_to_msec(struct timeval *time)
{
	return time->tv_sec * 1000 + time->tv_usec / 1000;
}

static void
handle_key_event(struct evdev_device *device, struct input_event *ev)
{
	uint32_t time = timeval_to_msec(&ev->time);
	uint32_t state;

	if ((ev->code >= BTN_MISC && ev->code <= BTN_GEAR_UP) || ev->code >= BTN_TRIGGER_HAPPY) {
		state = ev->value ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED;
		device->handler->button(time, ev->code, state);
		if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
			/* qemu generates GEAR_UP/GEAR_DOWN events on scroll, so
			 * pass those through as axis events. */
			switch (ev->code) {
			case BTN_GEAR_DOWN:
				device->handler->axis(time, WL_POINTER_AXIS_VERTICAL_SCROLL, wl_fixed_from_int(AXIS_STEP_DISTANCE));
				break;
			case BTN_GEAR_UP:
				device->handler->axis(time, WL_POINTER_AXIS_VERTICAL_SCROLL, wl_fixed_from_int(-AXIS_STEP_DISTANCE));
				break;
			}
		}
	} else {
		state = ev->value ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED;
		device->handler->key(time, ev->code, state);
	}
}

static void
handle_rel_event(struct evdev_device *device, struct input_event *ev)
{
	uint32_t time = timeval_to_msec(&ev->time);
	uint32_t axis, amount;

	if (!(device->capabilities & WL_SEAT_CAPABILITY_POINTER))
		return;

	switch (ev->code) {
	case REL_X:
		device->rel.dx += ev->value;
		device->rel.pending = true;
		return;
	case REL_Y:
		device->rel.dy += ev->value;
		device->rel.pending = true;
		return;
	case REL_WHEEL:
		axis = WL_POINTER_AXIS_VERTICAL_SCROLL;
		amount = -AXIS_STEP_DISTANCE * wl_fixed_from_int(ev->value);
		break;
	case REL_HWHEEL:
		axis = WL_POINTER_AXIS_HORIZONTAL_SCROLL;
		amount = AXIS_STEP_DISTANCE * wl_fixed_from_int(ev->value);
		break;
	default:
		return;
	}

	device->handler->axis(time, axis, amount);
}

static void
handle_abs_event(struct evdev_device *device, struct input_event *ev)
{
	if (!(device->capabilities & WL_SEAT_CAPABILITY_POINTER))
		return;

	switch (ev->code) {
	case ABS_X:
		device->abs.x = ev->value;
		device->abs.pending = true;
		break;
	case ABS_Y:
		device->abs.y = ev->value;
		device->abs.pending = true;
		break;
	}
}

static void (*event_handlers[])(struct evdev_device *device, struct input_event *ev) = {
	[EV_KEY] = handle_key_event,
	[EV_REL] = handle_rel_event,
	[EV_ABS] = handle_abs_event,
};

static bool
is_motion_event(struct input_event *ev)
{
	return (ev->type == EV_REL && (ev->code == REL_X || ev->code == REL_Y))
	    || (ev->type == EV_ABS && (ev->code == ABS_X || ev->code == ABS_Y));
}

static void
handle_motion_events(struct evdev_device *device, uint32_t time)
{
	if (device->abs.pending) {
		int32_t x = device->abs.x - device->abs.info.x->minimum;
		int32_t max_x = device->abs.info.x->maximum - device->abs.info.x->minimum;
		int32_t y = device->abs.y - device->abs.info.y->minimum;
		int32_t max_y = device->abs.info.y->maximum - device->abs.info.y->minimum;

		device->handler->absolute_motion(time, x, max_x, y, max_y);
		device->abs.pending = false;
	}
	if (device->rel.pending) {
		wl_fixed_t dx = wl_fixed_from_int(device->rel.dx);
		wl_fixed_t dy = wl_fixed_from_int(device->rel.dy);

		device->handler->relative_motion(time, dx, dy);
		device->rel.pending = false;
		device->rel.dx = 0;
		device->rel.dy = 0;
	}
}

static void
handle_event(struct evdev_device *device, struct input_event *ev)
{
	if (!is_motion_event(ev))
		handle_motion_events(device, timeval_to_msec(&ev->time));

	if (ev->type < ARRAY_SIZE(event_handlers) && event_handlers[ev->type])
		event_handlers[ev->type](device, ev);
}

static void
close_device(struct evdev_device *device)
{
	wl_event_source_remove(device->source);
	close(device->fd);
	device->source = NULL;
	device->fd = -1;
}

static int
handle_data(int fd, uint32_t mask, void *data)
{
	struct evdev_device *device = data;
	struct input_event event;
	unsigned flags = device->needs_sync ? LIBEVDEV_READ_FLAG_FORCE_SYNC : LIBEVDEV_READ_FLAG_NORMAL;
	int ret;

	device->needs_sync = false;

	for (;;) {
		ret = libevdev_next_event(device->dev, flags, &event);

		switch (ret) {
		case LIBEVDEV_READ_STATUS_SUCCESS:
			handle_event(device, &event);
			break;
		case LIBEVDEV_READ_STATUS_SYNC:
			while (ret == LIBEVDEV_READ_STATUS_SYNC) {
				ret = libevdev_next_event(device->dev, LIBEVDEV_READ_FLAG_SYNC, &event);

				if (ret < 0)
					goto done;

				handle_event(device, &event);
			}
			break;
		default:
			goto done;
		}
	}

done:
	if (ret == -ENODEV)
		close_device(device);

	handle_motion_events(device, timeval_to_msec(&event.time));

	return 1;
}

struct evdev_device *
evdev_device_new(const char *path, const struct evdev_device_handler *handler)
{
	struct evdev_device *device;

	if (!(device = malloc(sizeof *device)))
		goto error0;

	device->fd = launch_open_device(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);

	if (device->fd == -1) {
		ERROR("Failed to open input device at %s\n", path);
		goto error1;
	}

	if (!(device->path = strdup(path)))
		goto error2;

	if (libevdev_new_from_fd(device->fd, &device->dev) != 0) {
		ERROR("Failed to create libevdev device\n");
		goto error3;
	}

	if (libevdev_set_clock_id(device->dev, CLOCK_MONOTONIC) != 0) {
		ERROR("Failed to set libevdev device to monotonic clock\n");
		goto error4;
	}

	device->source = wl_event_loop_add_fd(swc.event_loop, device->fd, WL_EVENT_READABLE, handle_data, device);

	if (!device->source) {
		ERROR("Failed to add event source\n");
		goto error4;
	}

	DEBUG("Adding device %s\n", libevdev_get_name(device->dev));

	device->needs_sync = false;
	device->handler = handler;
	device->capabilities = 0;
	device->abs.info.x = libevdev_get_abs_info(device->dev, ABS_X);
	device->abs.info.y = libevdev_get_abs_info(device->dev, ABS_Y);
	device->abs.pending = false;
	memset(&device->rel, 0, sizeof(device->rel));

	if (libevdev_has_event_code(device->dev, EV_KEY, KEY_ENTER)) {
		device->capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
		DEBUG("\tThis device is a keyboard\n");
	}

	if (libevdev_has_event_code(device->dev, EV_REL, REL_X)
	 && libevdev_has_event_code(device->dev, EV_REL, REL_Y)
	 && libevdev_has_event_code(device->dev, EV_KEY, BTN_MOUSE))
	{
		device->capabilities |= WL_SEAT_CAPABILITY_POINTER;
		DEBUG("\tThis device is a pointer\n");
	}

	if (libevdev_has_event_code(device->dev, EV_ABS, ABS_X)
	 && libevdev_has_event_code(device->dev, EV_ABS, ABS_Y)
	 && libevdev_has_event_code(device->dev, EV_KEY, BTN_MOUSE)
	 && !libevdev_has_event_code(device->dev, EV_KEY, BTN_TOOL_FINGER))
	{
		device->capabilities |= WL_SEAT_CAPABILITY_POINTER;
	}

	/* XXX: touch devices */

	return device;

error4:
	libevdev_free(device->dev);
error3:
	free(device->path);
error2:
	close(device->fd);
error1:
	free(device);
error0:
	return NULL;
}

void
evdev_device_destroy(struct evdev_device *device)
{
	if (device->source)
		close_device(device);

	libevdev_free(device->dev);
	free(device->path);
	free(device);
}

bool
evdev_device_reopen(struct evdev_device *device)
{
	if (device->source)
		close_device(device);

	device->fd = launch_open_device(device->path, O_RDWR | O_NONBLOCK | O_CLOEXEC);

	if (device->fd == -1) {
		WARNING("Failed to reopen input device at %s: %s\n", device->path, strerror(errno));
		goto error0;
	}

	if (libevdev_change_fd(device->dev, device->fd) == -1) {
		ERROR("Failed to update libevdev fd\n");
		goto error1;
	}

	/* According to libevdev documentation, after changing the fd for the device,
	 * you should force a sync to bring it's state up to date. */
	device->needs_sync = true;
	device->source = wl_event_loop_add_fd(swc.event_loop, device->fd, WL_EVENT_READABLE, handle_data, device);

	if (!device->source) {
		ERROR("Failed to create event source\n");
		goto error1;
	}

	return true;

error1:
	close(device->fd);
	device->fd = -1;
error0:
	return false;
}
