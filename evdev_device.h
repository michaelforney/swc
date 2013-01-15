#ifndef SWC_EVDEV_DEVICE_H
#define SWC_EVDEV_DEVICE_H 1

#include <stdbool.h>
#include <libudev.h>
#include <linux/input.h>
#include <wayland-server.h>

struct swc_evdev_device
{
    struct swc_seat * seat;

    int fd;
    char * model, * vendor;

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
        bool pending;
    } rel;

    uint32_t capabilities;

    struct wl_event_source * source;
    struct wl_list link;
};

bool swc_evdev_device_initialize(struct swc_evdev_device * device,
                                 struct swc_seat * seat,
                                 struct udev_device * udev_device);

void swc_evdev_device_finish(struct swc_evdev_device * device);

void swc_evdev_device_add_event_sources(struct swc_evdev_device * device,
                                        struct wl_event_loop * event_loop);

#endif

