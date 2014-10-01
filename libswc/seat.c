/* swc: libswc/seat.c
 *
 * Copyright (c) 2013, 2014 Michael Forney
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

#include "seat.h"
#include "compositor.h"
#include "data_device.h"
#include "evdev_device.h"
#include "event.h"
#include "internal.h"
#include "keyboard.h"
#include "launch.h"
#include "pointer.h"
#include "util.h"

#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef ENABLE_LIBINPUT
# include <libudev.h>
# include <libinput.h>
#endif

static struct
{
    char * name;
    uint32_t capabilities;

#ifdef ENABLE_LIBINPUT
    struct udev * udev;
    struct libinput * libinput;
    struct wl_event_source * libinput_source;
#else
    struct wl_list devices;
#endif

    struct keyboard keyboard;
    struct pointer pointer;
    struct data_device data_device;

    struct wl_global * global;
    struct wl_list resources;
} seat;

const struct swc_seat swc_seat = {
    .pointer = &seat.pointer,
    .keyboard = &seat.keyboard,
    .data_device = &seat.data_device
};

static void handle_key(uint32_t time, uint32_t key, uint32_t state)
{
    keyboard_handle_key(&seat.keyboard, time, key, state);
}

static void handle_button(uint32_t time, uint32_t button, uint32_t state)
{
    pointer_handle_button(&seat.pointer, time, button, state);
}

static void handle_axis(uint32_t time, uint32_t axis, wl_fixed_t amount)
{
    pointer_handle_axis(&seat.pointer, time, axis, amount);
}

static void handle_relative_motion(uint32_t time, wl_fixed_t dx, wl_fixed_t dy)
{
    pointer_handle_relative_motion(&seat.pointer, time, dx, dy);
}

static void handle_keyboard_focus_event(struct wl_listener * listener,
                                        void * data)
{
    struct swc_event * event = data;
    struct input_focus_event_data * event_data = event->data;

    switch (event->type)
    {
        case INPUT_FOCUS_EVENT_CHANGED:
            if (event_data->new)
            {
                struct wl_client * client = wl_resource_get_client
                    (event_data->new->surface->resource);

                /* Offer the selection to the new focus. */
                data_device_offer_selection(&seat.data_device, client);
            }
            break;
    }
}

static struct wl_listener keyboard_focus_listener = {
    .notify = &handle_keyboard_focus_event
};

static void handle_data_device_event(struct wl_listener * listener, void * data)
{
    struct swc_event * event = data;

    switch (event->type)
    {
        case DATA_DEVICE_EVENT_SELECTION_CHANGED:
            if (seat.keyboard.focus.resource)
            {
                struct wl_client * client
                    = wl_resource_get_client(seat.keyboard.focus.resource);
                data_device_offer_selection(&seat.data_device, client);
            }
            break;
    }
}

static struct wl_listener data_device_listener = {
    .notify = &handle_data_device_event
};

static void handle_launch_event(struct wl_listener * listener, void * data)
{
    struct swc_event * event = data;

    switch (event->type)
    {
        case SWC_LAUNCH_EVENT_DEACTIVATED:
#ifdef ENABLE_LIBINPUT
            libinput_suspend(seat.libinput);
#endif
            keyboard_reset(&seat.keyboard);
            break;
        case SWC_LAUNCH_EVENT_ACTIVATED:
        {
#ifdef ENABLE_LIBINPUT
            if (libinput_resume(seat.libinput) != 0)
                WARNING("Failed to resume libinput context\n");
#else
            struct swc_evdev_device * device, * next;

            /* Re-open all input devices */
            wl_list_for_each_safe(device, next, &seat.devices, link)
            {
                if (!swc_evdev_device_reopen(device))
                {
                    wl_list_remove(&device->link);
                    swc_evdev_device_destroy(device);
                }
            }
#endif
            break;
        }
    }
}

static struct wl_listener launch_listener = {
    .notify = &handle_launch_event
};

/* Wayland Seat Interface */
static void get_pointer(struct wl_client * client,
                        struct wl_resource * resource, uint32_t id)
{
    pointer_bind(&seat.pointer, client, wl_resource_get_version(resource), id);
}

static void get_keyboard(struct wl_client * client,
                         struct wl_resource * resource, uint32_t id)
{
    keyboard_bind(&seat.keyboard, client,
                  wl_resource_get_version(resource), id);
}

static void get_touch(struct wl_client * client, struct wl_resource * resource,
               uint32_t id)
{
    /* XXX: Implement */
}

static struct wl_seat_interface seat_implementation = {
    .get_pointer = &get_pointer,
    .get_keyboard = &get_keyboard,
    .get_touch = &get_touch
};

static void bind_seat(struct wl_client * client, void * data, uint32_t version,
                      uint32_t id)
{
    struct wl_resource * resource;

    if (version >= 4)
        version = 4;

    resource = wl_resource_create(client, &wl_seat_interface, version, id);
    wl_resource_set_implementation(resource, &seat_implementation, NULL,
                                   &swc_remove_resource);
    wl_list_insert(&seat.resources, wl_resource_get_link(resource));

    if (version >= 2)
        wl_seat_send_name(resource, seat.name);

    wl_seat_send_capabilities(resource, seat.capabilities);
}

static void update_capabilities(uint32_t capabilities)
{
    if (~seat.capabilities & capabilities)
    {
        struct wl_resource * resource;

        seat.capabilities |= capabilities;
        wl_list_for_each(resource, &seat.resources, link)
            wl_seat_send_capabilities(resource, seat.capabilities);
    }
}

#ifdef ENABLE_LIBINPUT
static int open_restricted(const char * path, int flags, void * user_data)
{
    return swc_launch_open_device(path, flags);
}

static void close_restricted(int fd, void * user_data)
{
    close(fd);
}

const struct libinput_interface libinput_interface = {
    .open_restricted = &open_restricted,
    .close_restricted = &close_restricted,
};

static uint32_t device_capabilities(struct libinput_device * device)
{
    uint32_t capabilities = 0;

    if (libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_KEYBOARD))
        capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
    if (libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_POINTER))
        capabilities |= WL_SEAT_CAPABILITY_POINTER;
    /* TODO: Add touch device support
    if (libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_TOUCH))
        capabilities |= WL_SEAT_CAPABILITY_TOUCH;
    */

    return capabilities;
}

static int handle_libinput_data(int fd, uint32_t mask, void * data)
{
    struct libinput_event * generic_event;

    if (libinput_dispatch(seat.libinput) != 0)
    {
        WARNING("libinput_dispatch failed: %s\n", strerror(errno));
        return 0;
    }

    while ((generic_event = libinput_get_event(seat.libinput)))
    {
        switch (libinput_event_get_type(generic_event))
        {
            case LIBINPUT_EVENT_DEVICE_ADDED:
            {
                struct libinput_device * device;

                device = libinput_event_get_device(generic_event);
                update_capabilities(device_capabilities(device));
                break;
            }
            case LIBINPUT_EVENT_KEYBOARD_KEY:
            {
                struct libinput_event_keyboard * event;

                event = libinput_event_get_keyboard_event(generic_event);
                handle_key(libinput_event_keyboard_get_time(event),
                           libinput_event_keyboard_get_key(event),
                           libinput_event_keyboard_get_key_state(event));
                break;
            }
            case LIBINPUT_EVENT_POINTER_MOTION:
            {
                struct libinput_event_pointer * event;
                wl_fixed_t dx, dy;

                event = libinput_event_get_pointer_event(generic_event);
                dx = wl_fixed_from_double(libinput_event_pointer_get_dx(event));
                dy = wl_fixed_from_double(libinput_event_pointer_get_dy(event));
                handle_relative_motion(libinput_event_pointer_get_time(event),
                                       dx, dy);
                break;
            }
            case LIBINPUT_EVENT_POINTER_BUTTON:
            {
                struct libinput_event_pointer * event;

                event = libinput_event_get_pointer_event(generic_event);
                handle_button(libinput_event_pointer_get_time(event),
                              libinput_event_pointer_get_button(event),
                              libinput_event_pointer_get_button_state(event));
                break;
            }
            case LIBINPUT_EVENT_POINTER_AXIS:
            {
                struct libinput_event_pointer * event;
                wl_fixed_t amount;

                event = libinput_event_get_pointer_event(generic_event);
                amount = wl_fixed_from_double
                    (libinput_event_pointer_get_axis_value(event));
                handle_axis(libinput_event_pointer_get_time(event),
                            libinput_event_pointer_get_axis(event), amount);
                break;
            }
            default:
                break;
        }

        libinput_event_destroy(generic_event);
    }

    return 0;
}

bool initialize_libinput(const char * seat_name)
{
    if (!(seat.udev = udev_new()))
    {
        ERROR("Could not create udev context\n");
        goto error0;
    }

    seat.libinput = libinput_udev_create_context(&libinput_interface, NULL,
                                                 seat.udev);

    if (!seat.libinput)
    {
        ERROR("Could not create libinput context\n");
        goto error1;
    }

    if (libinput_udev_assign_seat(seat.libinput, seat_name) != 0)
    {
        ERROR("Failed to assign seat to libinput context\n");
        goto error2;
    }

    seat.libinput_source = wl_event_loop_add_fd
        (swc.event_loop, libinput_get_fd(seat.libinput), WL_EVENT_READABLE,
         &handle_libinput_data, NULL);

    if (!seat.libinput_source)
    {
        ERROR("Could not create event source for libinput\n");
        goto error2;
    }

    return true;

  error2:
    libinput_unref(seat.libinput);
  error1:
    udev_unref(seat.udev);
  error0:
    return false;
}

void finalize_libinput()
{
    wl_event_source_remove(seat.libinput_source);
    libinput_unref(seat.libinput);
    udev_unref(seat.udev);
}
#else
const static struct swc_evdev_device_handler evdev_handler = {
    .key = &handle_key,
    .button = &handle_button,
    .axis = &handle_axis,
    .relative_motion = &handle_relative_motion,
};

static void add_device(const char * path)
{
    struct swc_evdev_device * device;

    if (!(device = swc_evdev_device_new(path, &evdev_handler)))
    {
        ERROR("Could not create evdev device\n");
        return;
    }

    update_capabilities(device->capabilities);
    wl_list_insert(&seat.devices, &device->link);
}

static int select_device(const struct dirent * entry)
{
    unsigned num;

    return sscanf(entry->d_name, "event%u", &num) == 1;
}

static bool add_devices()
{
    struct dirent ** devices;
    int num_devices;
    char path[64];
    unsigned index;

    num_devices = scandir("/dev/input", &devices, &select_device, &alphasort);

    if (num_devices == -1)
    {
        ERROR("Failed to scan /dev/input for event devices\n");
        return false;
    }

    for (index = 0; index < num_devices; ++index)
    {
        snprintf(path, sizeof path, "/dev/input/%s", devices[index]->d_name);
        free(devices[index]);
        add_device(path);
    }

    free(devices);

    return true;
}
#endif

bool swc_seat_initialize(const char * seat_name)
{
    if (!(seat.name = strdup(seat_name)))
    {
        ERROR("Could not allocate seat name string\n");
        goto error0;
    }

    seat.global = wl_global_create(swc.display, &wl_seat_interface, 4,
                                   NULL, &bind_seat);

    if (!seat.global)
        goto error1;

    seat.capabilities = 0;
    wl_list_init(&seat.resources);
    wl_signal_add(&swc.launch->event_signal, &launch_listener);

    if (!data_device_initialize(&seat.data_device))
    {
        ERROR("Could not initialize data device\n");
        goto error2;
    }

    wl_signal_add(&seat.data_device.event_signal, &data_device_listener);

    if (!keyboard_initialize(&seat.keyboard))
    {
        ERROR("Could not initialize keyboard\n");
        goto error3;
    }

    wl_signal_add(&seat.keyboard.focus.event_signal, &keyboard_focus_listener);

    if (!pointer_initialize(&seat.pointer))
    {
        ERROR("Could not initialize pointer\n");
        goto error4;
    }

#ifdef ENABLE_LIBINPUT
    if (!initialize_libinput(seat.name))
        goto error5;
#else
    wl_list_init(&seat.devices);

    if (!add_devices())
        goto error5;
#endif

    return true;

  error5:
    pointer_finalize(&seat.pointer);
  error4:
    keyboard_finalize(&seat.keyboard);
  error3:
    data_device_finalize(&seat.data_device);
  error2:
    wl_global_destroy(seat.global);
  error1:
    free(seat.name);
  error0:
    return false;
}

void swc_seat_finalize()
{
#ifdef ENABLE_LIBINPUT
    finalize_libinput();
#else
    struct swc_evdev_device * device, * tmp;
    wl_list_for_each_safe(device, tmp, &seat.devices, link)
        swc_evdev_device_destroy(device);
#endif

    pointer_finalize(&seat.pointer);
    keyboard_finalize(&seat.keyboard);
    data_device_finalize(&seat.data_device);

    wl_global_destroy(seat.global);
    free(seat.name);
}

