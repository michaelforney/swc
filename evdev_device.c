#include "evdev_device.h"

#include "seat.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define BITS(var, n) uint64_t var[((n)-1)/64+1]
#define TEST_BIT(var, n) \
    (((var)[(n)/((sizeof (var)[0])*8)] >> ((n)%((sizeof (var)[0])*8))) & 1)

static inline uint32_t timeval_to_msec(struct timeval * time)
{
    return time->tv_sec * 1000 + time->tv_usec / 1000;
}

static void handle_key_event(struct swc_evdev_device * device,
                             struct input_event * event)
{
    uint32_t time = timeval_to_msec(&event->time);

    if ((event->code >= BTN_MISC && event->code <= BTN_GEAR_UP)
        || event->code >= BTN_TRIGGER_HAPPY)
    {
        swc_seat_handle_button(device->seat, time, event->value,
                               event->value ? WL_POINTER_BUTTON_STATE_PRESSED
                                            : WL_POINTER_BUTTON_STATE_RELEASED);
    }
    else
    {
        swc_seat_handle_key(device->seat, time, event->code,
                            event->value ? WL_KEYBOARD_KEY_STATE_PRESSED
                                         : WL_KEYBOARD_KEY_STATE_RELEASED);
    }
}

static void handle_rel_event(struct swc_evdev_device * device,
                             struct input_event * event)
{
    //printf("rel event\n");

    switch (event->code)
    {
        case REL_X:
            //printf("rel_x: %d\n", event->value);
            break;
        case REL_Y:
            //printf("rel_y: %d\n", event->value);
            break;
    }
}

static void handle_abs_event(struct swc_evdev_device * device,
                             struct input_event * event)
{
    printf("abs event\n");
}

static void (* event_handlers[])(struct swc_evdev_device * device,
                                 struct input_event * event) = {
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
}

static void process_events(struct swc_evdev_device * device,
                           struct input_event * events, uint32_t event_count)
{
    struct input_event * event, * end = events + event_count;

    for (event = events; event != end; ++event)
    {
        if (!is_motion_event(event))
            handle_motion_events(device, timeval_to_msec(&event->time));

        /*
        printf("processing event, type: %u, code: %u, value: %d\n",
               event->type, event->code, event->value);
        */

        if (event->type < (sizeof event_handlers / sizeof event_handlers[0])
            && event_handlers[event->type])
        {
            event_handlers[event->type](device, event);
        }
    }
}

static int handle_data(int fd, uint32_t mask, void * data)
{
    struct swc_evdev_device * device = data;
    struct input_event events[32];
    ssize_t bytes_read;

    do
    {
        bytes_read = read(fd, events, sizeof events);

        /* Stop on error */
        if (bytes_read == -1)
            return 1;

        process_events(device, events, bytes_read / sizeof events[0]);
    } while (bytes_read > 0);

    return 1;
}

bool swc_evdev_device_initialize(struct swc_evdev_device * device,
                                 struct swc_seat * seat,
                                 struct udev_device * udev_device)
{
    const char * path, * model, * vendor;
    BITS(ev_bits, EV_MAX);
    uint32_t index;

    path = udev_device_get_devnode(udev_device);
    model = udev_device_get_property_value(udev_device, "ID_MODEL")
        ?: "unknown";
    vendor = udev_device_get_property_value(udev_device, "ID_VENDOR")
        ?: "unknown";

    device->seat = seat;
    device->model = strdup(model);
    device->vendor = strdup(vendor);
    device->fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);

    if (device->fd == -1)
    {
        printf("couldn't open input device at %s\n", path);
        goto error_base;
    }

    printf("adding device %s %s\n", device->vendor, device->model);

    ioctl(device->fd, EVIOCGBIT(0, sizeof ev_bits), &ev_bits);

    device->capabilities = 0;
    /* Currently, I don't care about touch devices. */

    if (udev_device_get_property_value(udev_device, "ID_INPUT_KEYBOARD"))
    {
        device->capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
        printf("\tthis device is a keyboard\n");
    }

    if (udev_device_get_property_value(udev_device, "ID_INPUT_MOUSE")
        || udev_device_get_property_value(udev_device, "ID_INPUT_TOUCHPAD"))
    {
        device->capabilities |= WL_SEAT_CAPABILITY_POINTER;
        printf("\tthis device is a pointer\n");
    }

    if (device->capabilities & WL_SEAT_CAPABILITY_POINTER)
    {
        /* Check if the device has relative motion. */
        if (TEST_BIT(ev_bits, EV_REL))
        {
            BITS(rel_bits, REL_MAX);

            ioctl(device->fd, EVIOCGBIT(EV_REL, sizeof rel_bits), &rel_bits);

            if (TEST_BIT(rel_bits, REL_X) || TEST_BIT(rel_bits, REL_Y))
            {
            }
        }

        /* Check if the device has absolute motion. */
        if (TEST_BIT(ev_bits, EV_ABS))
        {
            BITS(abs_bits, ABS_MAX);

            ioctl(device->fd, EVIOCGBIT(EV_ABS, sizeof abs_bits), &abs_bits);

            if (TEST_BIT(abs_bits, ABS_X))
                ioctl(device->fd, EVIOCGABS(ABS_X), &device->abs.info.x);
            if (TEST_BIT(abs_bits, ABS_Y))
                ioctl(device->fd, EVIOCGABS(ABS_X), &device->abs.info.y);
        }
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
    free(device->model);
    free(device->vendor);
    close(device->fd);
}

void swc_evdev_device_add_event_sources(struct swc_evdev_device * device,
                                        struct wl_event_loop * event_loop)
{
    printf("adding event source for %s %s\n", device->vendor, device->model);
    device->source
        = wl_event_loop_add_fd(event_loop, device->fd, WL_EVENT_READABLE,
                               handle_data, device);
}

