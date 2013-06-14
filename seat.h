#ifndef SWC_SEAT_H
#define SWC_SEAT_H 1

#include "xkb.h"
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

    struct swc_xkb xkb;

    struct wl_list resources;
    struct wl_signal destroy_signal;

    struct swc_keyboard keyboard;
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

void swc_seat_handle_key(struct swc_seat * seat, uint32_t time, uint32_t key,
                         uint32_t state);

void swc_seat_handle_button(struct swc_seat * seat, uint32_t time,
                            uint32_t button, uint32_t state);

/* Wayland Seat Interface */
extern struct wl_seat_interface swc_seat_interface;

void swc_seat_get_pointer(struct wl_client * client,
                          struct wl_resource * resource, uint32_t id);

void swc_seat_get_keyboard(struct wl_client * client,
                           struct wl_resource * resource, uint32_t id);

void swc_seat_get_touch(struct wl_client * client,
                        struct wl_resource * resource, uint32_t id);

#endif

