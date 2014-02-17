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
#include "data_device.h"
#include "evdev_device.h"
#include "event.h"
#include "internal.h"
#include "keyboard.h"
#include "launch.h"
#include "pointer.h"
#include "util.h"

#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef ENABLE_HOTPLUGGING
# include <libudev.h>
#endif

static struct
{
    char * name;
    uint32_t capabilities;

#ifdef ENABLE_HOTPLUGGING
    struct udev * udev;
    struct udev_monitor * monitor;
    struct wl_event_source * monitor_source;
#endif

    struct swc_keyboard keyboard;
    struct pointer pointer;
    struct swc_data_device data_device;

    struct wl_global * global;
    struct wl_list resources;
    struct wl_list devices;
} seat;

const struct swc_seat swc_seat = {
    .pointer = &seat.pointer,
    .keyboard = &seat.keyboard,
    .data_device = &seat.data_device
};

static void handle_key(uint32_t time, uint32_t key, uint32_t state)
{
    swc_keyboard_handle_key(&seat.keyboard, time, key, state);
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

const static struct swc_evdev_device_handler evdev_handler = {
    .key = &handle_key,
    .button = &handle_button,
    .axis = &handle_axis,
    .relative_motion = &handle_relative_motion
};

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
                struct wl_client * client
                    = wl_resource_get_client(event_data->new->resource);

                /* Offer the selection to the new focus. */
                swc_data_device_offer_selection(&seat.data_device, client);
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
        case SWC_DATA_DEVICE_EVENT_SELECTION_CHANGED:
            if (seat.keyboard.focus.resource)
            {
                struct wl_client * client
                    = wl_resource_get_client(seat.keyboard.focus.resource);
                swc_data_device_offer_selection(&seat.data_device, client);
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
    struct swc_evdev_device * device, * next;

    switch (event->type)
    {
        case SWC_LAUNCH_EVENT_ACTIVATED:
            /* Re-open all input devices */
            wl_list_for_each_safe(device, next, &seat.devices, link)
            {
                if (!swc_evdev_device_reopen(device))
                {
                    wl_list_remove(&device->link);
                    swc_evdev_device_destroy(device);
                }
            }

            break;
    }
}

static struct wl_listener launch_listener = {
    .notify = &handle_launch_event
};

/* Wayland Seat Interface */
static void get_pointer(struct wl_client * client, struct wl_resource * resource,
                        uint32_t id)
{
    pointer_bind(&seat.pointer, client, id);
}

static void get_keyboard(struct wl_client * client, struct wl_resource * resource,
                         uint32_t id)
{
    swc_keyboard_bind(&seat.keyboard, client, id);
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

    if (version >= 2)
        version = 2;

    resource = wl_resource_create(client, &wl_seat_interface, version, id);
    wl_resource_set_implementation(resource, &seat_implementation, NULL,
                                   &swc_remove_resource);
    wl_list_insert(&seat.resources, wl_resource_get_link(resource));

    if (version >= 2)
        wl_seat_send_name(resource, seat.name);

    wl_seat_send_capabilities(resource, seat.capabilities);
}

static void add_device(const char * path)
{
    struct swc_evdev_device * device;

    if (!(device = swc_evdev_device_new(path, &evdev_handler)))
    {
        ERROR("Could not create evdev device\n");
        return;
    }

    if (~seat.capabilities & device->capabilities)
    {
        struct wl_resource * resource;

        seat.capabilities |= device->capabilities;
        wl_list_for_each(resource, &seat.resources, link)
            wl_seat_send_capabilities(resource, seat.capabilities);
    }

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

#ifdef ENABLE_HOTPLUGGING
static int handle_monitor_data(int fd, uint32_t mask, void * data)
{
    struct udev_device * udev_device;
    const char * path, * action, * sysname;
    unsigned num;

    if (!(udev_device = udev_monitor_receive_device(seat.monitor)))
        return 0;

    if (!(action = udev_device_get_action(udev_device)))
        goto done;

    sysname = udev_device_get_sysname(udev_device);

    if (sscanf(sysname, "event%u", &num) != 1)
        goto done;

    path = udev_device_get_devnode(udev_device);

    if (strcmp(action, "add") == 0)
        add_device(path);
    else if (strcmp(action, "remove") == 0)
    {
        struct swc_evdev_device * device, * next;

        wl_list_for_each_safe(device, next, &seat.devices, link)
        {
            if (strcmp(device->path, path) == 0)
            {
                wl_list_remove(&device->link);
                swc_evdev_device_destroy(device);
                break;
            }
        }
    }

  done:
    udev_device_unref(udev_device);
    return 0;
}

bool initialize_monitor()
{
    int fd;

    if (!(seat.udev = udev_new()))
    {
        ERROR("Could not create udev context\n");
        goto error0;
    }

    if (!(seat.monitor = udev_monitor_new_from_netlink(seat.udev, "udev")))
    {
        ERROR("Could not create udev monitor\n");
        goto error1;
    }

    if (udev_monitor_filter_add_match_subsystem_devtype(seat.monitor,
                                                        "input", NULL) != 0)
    {
        ERROR("Could not set up udev monitor filter\n");
        goto error2;
    }

    if (udev_monitor_enable_receiving(seat.monitor) != 0)
    {
        ERROR("Could not enable receiving for udev monitor\n");
        goto error2;
    }

    fd = udev_monitor_get_fd(seat.monitor);
    seat.monitor_source = wl_event_loop_add_fd
        (swc.event_loop, fd, WL_EVENT_READABLE, &handle_monitor_data, NULL);

    if (!seat.monitor_source)
    {
        ERROR("Could not create event source for udev monitor\n");
        goto error2;
    }

    return true;

  error2:
    udev_monitor_unref(seat.monitor);
  error1:
    udev_unref(seat.udev);
  error0:
    return false;
}

void finalize_monitor()
{
    wl_event_source_remove(seat.monitor_source);
    udev_monitor_unref(seat.monitor);
    udev_unref(seat.udev);
}
#endif

bool swc_seat_initialize(const char * seat_name)
{
    if (!(seat.name = strdup(seat_name)))
    {
        ERROR("Could not allocate seat name string\n");
        goto error0;
    }

    seat.global = wl_global_create(swc.display, &wl_seat_interface, 2,
                                       NULL, &bind_seat);

    if (!seat.global)
        goto error1;

    seat.capabilities = 0;
    wl_list_init(&seat.resources);
    wl_list_init(&seat.devices);
    wl_signal_add(&swc.launch->event_signal, &launch_listener);

    if (!swc_data_device_initialize(&seat.data_device))
    {
        ERROR("Could not initialize data device\n");
        goto error2;
    }

    wl_signal_add(&seat.data_device.event_signal, &data_device_listener);

    if (!swc_keyboard_initialize(&seat.keyboard))
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

#ifdef ENABLE_HOTPLUGGING
    if (!initialize_monitor())
        goto error5;
#endif

    if (!add_devices())
        goto error6;

    return true;

  error6:
#ifdef ENABLE_HOTPLUGGING
    finalize_monitor();
  error5:
#endif
    pointer_finalize(&seat.pointer);
  error4:
    swc_keyboard_finalize(&seat.keyboard);
  error3:
    swc_data_device_finalize(&seat.data_device);
  error2:
    wl_global_destroy(seat.global);
  error1:
    free(seat.name);
  error0:
    return false;
}

void swc_seat_finalize()
{
    struct swc_evdev_device * device, * tmp;

#ifdef ENABLE_HOTPLUGGING
    finalize_monitor();
#endif
    pointer_finalize(&seat.pointer);
    swc_keyboard_finalize(&seat.keyboard);

    wl_list_for_each_safe(device, tmp, &seat.devices, link)
        swc_evdev_device_destroy(device);

    wl_global_destroy(seat.global);
    free(seat.name);
}

