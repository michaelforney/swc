#ifndef SWC_EVDEV_DEVICE_H
#define SWC_EVDEV_DEVICE_H

#include <stdbool.h>
#include <linux/input.h>
#include <wayland-util.h>

struct swc_evdev_device;
struct wl_event_loop;

struct swc_evdev_device_handler
{
    void (* key)(uint32_t time, uint32_t key, uint32_t state);
    void (* button)(uint32_t time, uint32_t key, uint32_t state);
    void (* axis)(uint32_t time, uint32_t axis, wl_fixed_t amount);
    void (* relative_motion)(uint32_t time, wl_fixed_t dx, wl_fixed_t dy);
};

struct swc_evdev_device
{
    char * path;
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

struct swc_evdev_device * swc_evdev_device_new
    (const char * path, const struct swc_evdev_device_handler * handler);

void swc_evdev_device_destroy(struct swc_evdev_device * device);

void swc_evdev_device_add_event_sources(struct swc_evdev_device * device,
                                        struct wl_event_loop * event_loop);

bool swc_evdev_device_reopen(struct swc_evdev_device * device);

#endif

