#ifndef SWC_SEAT_H
#define SWC_SEAT_H 1

#include "xkb.h"
#include "data_device.h"
#include "keyboard.h"
#include "pointer.h"

#include <stdint.h>
#include <stdbool.h>
#include <libudev.h>
#include <wayland-server.h>

struct swc_seat
{
    char * name;
    uint32_t capabilities;

    struct wl_list resources;
    struct wl_signal destroy_signal;

    struct swc_data_device data_device;
    struct wl_listener data_device_listener;

    struct swc_keyboard keyboard;
    struct wl_listener keyboard_focus_listener;

    struct swc_pointer pointer;

    struct wl_list devices;
};

bool swc_seat_initialize(struct swc_seat * seat, struct udev * udev,
                         const char * seat_name);

void swc_seat_finish(struct swc_seat * seat);

void swc_seat_add_globals(struct swc_seat * seat, struct wl_display * display);

void swc_seat_add_event_sources(struct swc_seat * seat,
                                struct wl_event_loop * event_loop);

void swc_seat_add_devices(struct swc_seat * seat, struct udev * udev);


#endif

