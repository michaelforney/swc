#include "evdev_device.h"

#include "seat.h"
#include "event.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <libevdev/libevdev.h>

#define AXIS_STEP_DISTANCE 10
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

static inline uint32_t timeval_to_msec(struct timeval * time)
{
    return time->tv_sec * 1000 + time->tv_usec / 1000;
}

static void handle_key_event(struct swc_evdev_device * device,
                             struct input_event * input_event)
{
    uint32_t time = timeval_to_msec(&input_event->time);
    uint32_t state;

    if ((input_event->code >= BTN_MISC && input_event->code <= BTN_GEAR_UP)
        || input_event->code >= BTN_TRIGGER_HAPPY)
    {
        state = input_event->value ? WL_POINTER_BUTTON_STATE_PRESSED
                                   : WL_POINTER_BUTTON_STATE_RELEASED;
        device->handler->button(device->handler, time,
                                input_event->code, state);
    }
    else
    {
        state = input_event->value ? WL_KEYBOARD_KEY_STATE_PRESSED
                                   : WL_KEYBOARD_KEY_STATE_RELEASED;
        device->handler->key(device->handler, time, input_event->code, state);
    }
}

static void handle_rel_event(struct swc_evdev_device * device,
                             struct input_event * input_event)
{
    uint32_t time = timeval_to_msec(&input_event->time);
    uint32_t axis, amount;

    switch (input_event->code)
    {
        case REL_X:
            device->motion.rel.dx += input_event->value;
            device->motion.rel.pending = true;
            return;
        case REL_Y:
            device->motion.rel.dy += input_event->value;
            device->motion.rel.pending = true;
            return;
        case REL_WHEEL:
            axis = WL_POINTER_AXIS_VERTICAL_SCROLL;
            amount = -AXIS_STEP_DISTANCE * wl_fixed_from_int(input_event->value);
            break;
        case REL_HWHEEL:
            axis = WL_POINTER_AXIS_HORIZONTAL_SCROLL;
            amount = AXIS_STEP_DISTANCE * wl_fixed_from_int(input_event->value);
            break;
    }

    device->handler->axis(device->handler, time, axis, amount);
}

static void handle_abs_event(struct swc_evdev_device * device,
                             struct input_event * input_event)
{
    printf("abs event\n");
}

static void (* event_handlers[])(struct swc_evdev_device * device,
                                 struct input_event * input_event) = {
    [EV_KEY] = &handle_key_event,
    [EV_REL] = &handle_rel_event,
    [EV_ABS] = &handle_abs_event
};

static bool is_motion_event(struct input_event * event)
{
    return (event->type == EV_REL && (event->code == REL_X || event->code == REL_Y))
        || (event->type == EV_ABS && (event->code == ABS_X || event->code == ABS_Y));
}

static void handle_motion_events(struct swc_evdev_device * device,
                                 uint32_t time)
{
    if (device->motion.rel.pending)
    {
        wl_fixed_t dx = wl_fixed_from_int(device->motion.rel.dx);
        wl_fixed_t dy = wl_fixed_from_int(device->motion.rel.dy);

        device->handler->relative_motion(device->handler, time, dx, dy);

        device->motion.rel.pending = false;
        device->motion.rel.dx = 0;
        device->motion.rel.dy = 0;
    }
}

static int handle_data(int fd, uint32_t mask, void * data)
{
    struct swc_evdev_device * device = data;
    struct input_event event;
    int ret;

    do
    {
        ret = libevdev_next_event(device->dev, LIBEVDEV_READ_NORMAL, &event);

        while (ret == 1)
            ret = libevdev_next_event(device->dev, LIBEVDEV_READ_SYNC, &event);

        if (!is_motion_event(&event))
            handle_motion_events(device, timeval_to_msec(&event.time));

        if (event.type < ARRAY_SIZE(event_handlers)
            && event_handlers[event.type])
        {
            event_handlers[event.type](device, &event);
        }
    } while (ret != -EAGAIN);

    handle_motion_events(device, timeval_to_msec(&event.time));

    return 1;
}

bool swc_evdev_device_initialize(struct swc_evdev_device * device,
                                 const char * path,
                                 const struct swc_evdev_device_handler * handler)
{
    uint32_t index;

    device->fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    memset(&device->motion, 0, sizeof device->motion);

    if (device->fd == -1)
    {
        printf("couldn't open input device at %s\n", path);
        goto error_base;
    }

    if (libevdev_new_from_fd(device->fd, &device->dev) != 0)
    {
        fprintf(stderr, "Could not create libevdev device\n");
        goto error_fd;
    }

    printf("Adding device %s\n", libevdev_get_name(device->dev));

    device->handler = handler;
    device->capabilities = 0;
    /* XXX: touch devices */

    if (libevdev_has_event_code(device->dev, EV_KEY, KEY_ENTER))
    {
        device->capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
        printf("\tthis device is a keyboard\n");
    }

    if (libevdev_has_event_code(device->dev, EV_REL, REL_X)
        && libevdev_has_event_code(device->dev, EV_REL, REL_Y)
        && libevdev_has_event_code(device->dev, EV_KEY, BTN_MOUSE))
    {
        device->capabilities |= WL_SEAT_CAPABILITY_POINTER;
        printf("\tthis device is a pointer\n");
    }

    return true;

  error_fd:
    close(device->fd);
  error_base:
    return false;
}

void swc_evdev_device_finish(struct swc_evdev_device * device)
{
    wl_event_source_remove(device->source);
    libevdev_free(device->dev);
    close(device->fd);
}

void swc_evdev_device_add_event_sources(struct swc_evdev_device * device,
                                        struct wl_event_loop * event_loop)
{
    printf("Adding event source for %s\n", libevdev_get_name(device->dev));
    device->source
        = wl_event_loop_add_fd(event_loop, device->fd, WL_EVENT_READABLE,
                               handle_data, device);
}

