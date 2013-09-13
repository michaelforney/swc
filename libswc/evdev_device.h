#ifndef SWC_EVDEV_DEVICE_H
#define SWC_EVDEV_DEVICE_H 1

#include <stdbool.h>
#include <libudev.h>
#include <linux/input.h>
#include <wayland-server.h>

enum swc_evdev_device_event_type
{
    SWC_EVDEV_DEVICE_EVENT_KEY,
    SWC_EVDEV_DEVICE_EVENT_BUTTON,
    SWC_EVDEV_DEVICE_EVENT_RELATIVE_MOTION,
    SWC_EVDEV_DEVICE_EVENT_ABSOLUTE_MOTION,
    SWC_EVDEV_DEVICE_EVENT_AXIS_MOTION
};

struct swc_evdev_device_event_data
{
    uint32_t time;
    union
    {
        struct
        {
            wl_fixed_t dx, dy;
        } relative_motion;

        struct
        {
            wl_fixed_t amount;
            enum wl_pointer_axis axis;
        } axis_motion;

        struct
        {
            uint32_t key;
            enum wl_keyboard_key_state state;
        } key;

        struct
        {
            uint32_t button;
            enum wl_pointer_button_state state;
        } button;
    };
};

struct swc_evdev_device
{
    int fd;
    char * model, * vendor;

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
    struct wl_signal event_signal;
    struct wl_list link;
};

bool swc_evdev_device_initialize(struct swc_evdev_device * device,
                                 struct udev_device * udev_device);

void swc_evdev_device_finish(struct swc_evdev_device * device);

void swc_evdev_device_add_event_sources(struct swc_evdev_device * device,
                                        struct wl_event_loop * event_loop);

#endif

