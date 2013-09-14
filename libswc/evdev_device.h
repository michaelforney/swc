#ifndef SWC_EVDEV_DEVICE_H
#define SWC_EVDEV_DEVICE_H 1

#include <stdbool.h>
#include <linux/input.h>
#include <wayland-server.h>

struct libevdev_device;
struct swc_evdev_device;

struct swc_evdev_device_handler
{
    void (* key)(const struct swc_evdev_device_handler * handler,
                 uint32_t time, uint32_t key, uint32_t state);
    void (* button)(const struct swc_evdev_device_handler * handler,
                    uint32_t time, uint32_t key, uint32_t state);
    void (* axis)(const struct swc_evdev_device_handler * handler,
                  uint32_t time, uint32_t axis, wl_fixed_t amount);
    void (* relative_motion)(const struct swc_evdev_device_handler * handler,
                             uint32_t time, wl_fixed_t dx, wl_fixed_t dy);
};

struct swc_evdev_device
{
    int fd;
    struct libevdev * dev;

    const struct swc_evdev_device_handler * handler;

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

bool swc_evdev_device_initialize
    (struct swc_evdev_device * device, const char * path,
     const struct swc_evdev_device_handler * handler);

void swc_evdev_device_finish(struct swc_evdev_device * device);

void swc_evdev_device_add_event_sources(struct swc_evdev_device * device,
                                        struct wl_event_loop * event_loop);

#endif

