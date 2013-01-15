#include "seat.h"

#include "evdev_device.h"
#include "util.h"
#include "binding.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct wl_seat_interface swc_seat_interface = {
    .get_pointer = &swc_seat_get_pointer,
    .get_keyboard = &swc_seat_get_keyboard,
    .get_touch = &swc_seat_get_touch
};

static void bind_seat(struct wl_client * client, void * data, uint32_t version,
                      uint32_t id)
{
    struct swc_seat * seat = data;
    struct wl_resource * resource;

    resource = wl_client_add_object(client, &wl_seat_interface,
                                    &swc_seat_interface, id, seat);
    wl_list_insert(&seat->wayland.base_resource_list, &resource->link);
    resource->destroy = &swc_unbind_resource;

    wl_seat_send_capabilities(resource, seat->capabilities);
}

static void add_device(struct swc_seat * seat, struct udev_device * udev_device)
{
    const char * device_seat;
    const char * device_path;
    struct swc_evdev_device * evdev_device;

    device_seat = udev_device_get_property_value(udev_device, "ID_SEAT");

    /* If the ID_SEAT property is not set, the device belongs to seat0. */
    if (!device_seat)
        device_seat = "seat0";

    if (strcmp(device_seat, seat->name) != 0)
        return;

    evdev_device = malloc(sizeof *evdev_device);

    if (!swc_evdev_device_initialize(evdev_device, seat, udev_device))
    {
        free(evdev_device);
        return;
    }

    if (!(seat->capabilities & WL_SEAT_CAPABILITY_POINTER)
        && evdev_device->capabilities & WL_SEAT_CAPABILITY_POINTER)
    {
        printf("initializing pointer\n");
        wl_pointer_init(&seat->pointer);
        wl_seat_set_pointer(&seat->wayland, &seat->pointer);
    }

    if (!(seat->capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
        && evdev_device->capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
    {
        printf("initializing keyboard\n");
        wl_keyboard_init(&seat->keyboard);
        wl_seat_set_keyboard(&seat->wayland, &seat->keyboard);
    }

    seat->capabilities |= evdev_device->capabilities;

    wl_list_insert(&seat->devices, &evdev_device->link);
}

bool swc_seat_initialize(struct swc_seat * seat, struct udev * udev,
                         const char * seat_name)
{
    wl_seat_init(&seat->wayland);

    seat->name = strdup(seat_name);
    seat->capabilities = 0;
    seat->active_modifiers = 0;

    if (!swc_xkb_initialize(&seat->xkb))
    {
        printf("could not initialize XKB\n");
        goto error_name;
    }

    wl_list_init(&seat->devices);
    swc_seat_add_devices(seat, udev);

    return true;

  error_name:
    free(seat->name);
  error_base:
    return false;
}

void swc_seat_finish(struct swc_seat * seat)
{
    struct swc_evdev_device * device, * tmp;

    wl_seat_release(&seat->wayland);
    free(seat->name);
    swc_xkb_finish(&seat->xkb);

    wl_list_for_each_safe(device, tmp, &seat->devices, link)
    {
        swc_evdev_device_finish(device);
        free(device);
    }
}

void swc_seat_add_globals(struct swc_seat * seat, struct wl_display * display)
{
    wl_display_add_global(display, &wl_seat_interface, seat, &bind_seat);
}

void swc_seat_add_event_sources(struct swc_seat * seat,
                                struct wl_event_loop * event_loop)
{
    struct swc_evdev_device * device;

    wl_list_for_each(device, &seat->devices, link)
    {
        swc_evdev_device_add_event_sources(device, event_loop);
    }
}

void swc_seat_add_devices(struct swc_seat * seat, struct udev * udev)
{
    struct udev_enumerate * enumerate;
    struct udev_list_entry * entry;
    const char * path;
    struct udev_device * device;

    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_add_match_sysname(enumerate, "event[0-9]*");

    udev_enumerate_scan_devices(enumerate);

    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate))
    {
        path = udev_list_entry_get_name(entry);
        device = udev_device_new_from_syspath(udev, path);
        add_device(seat, device);
        udev_device_unref(device);
    }

    udev_enumerate_unref(enumerate);
}

void swc_seat_handle_key(struct swc_seat * seat, uint32_t time, uint32_t key,
                         uint32_t state)
{
    uint32_t * pressed_key;
    struct wl_keyboard * keyboard = &seat->keyboard;
    struct swc_xkb * xkb = &seat->xkb;
    enum xkb_key_direction direction;

    /* Update XKB state */
    direction = state == WL_KEYBOARD_KEY_STATE_PRESSED ? XKB_KEY_DOWN
                                                       : XKB_KEY_UP;

    /* Apparently these are offset by 8 in X. */
    xkb_state_update_key(xkb->state, key + 8, direction);

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
    {
        keyboard->grab_key = key;
        keyboard->grab_time = time;
        pressed_key = wl_array_add(&keyboard->keys, sizeof key);
        *pressed_key = key;
    }
    else
    {
        wl_array_for_each(pressed_key, &keyboard->keys)
        {
            if (*pressed_key == key)
            {
                /* Remove the key from the array */
                uint32_t bytes_to_copy = keyboard->keys.size + 1
                    - (((void *) pressed_key) - keyboard->keys.data);
                memmove(pressed_key, pressed_key + 1, bytes_to_copy);
                --keyboard->keys.size;
                break;
            }
        }
    }

    keyboard->grab->interface->key(keyboard->grab, time, key, state);

    {
        struct wl_keyboard * keyboard = &seat->keyboard;
        uint32_t mods_depressed, mods_latched, mods_locked, mods_active;
        uint32_t layout;

        mods_depressed = xkb_state_serialize_mods(xkb->state, XKB_STATE_DEPRESSED);
        mods_latched = xkb_state_serialize_mods(xkb->state, XKB_STATE_LATCHED);
        mods_locked = xkb_state_serialize_mods(xkb->state, XKB_STATE_LOCKED);
        mods_active = mods_depressed | mods_latched;

        layout = xkb_state_serialize_layout(xkb->state, XKB_STATE_LAYOUT_EFFECTIVE);

        if (mods_depressed != keyboard->modifiers.mods_depressed
            || mods_latched != keyboard->modifiers.mods_latched
            || mods_locked != keyboard->modifiers.mods_locked
            || layout != keyboard->modifiers.group)
        {
        }

        keyboard->modifiers.mods_depressed = mods_depressed;
        keyboard->modifiers.mods_latched = mods_latched;
        keyboard->modifiers.mods_locked = mods_locked;
        keyboard->modifiers.group = layout;

        seat->active_modifiers = 0;

        if (mods_active & (1 << xkb->indices.ctrl))
            seat->active_modifiers |= MOD_CTRL;
        if (mods_active & (1 << xkb->indices.alt))
            seat->active_modifiers |= MOD_ALT;
        if (mods_active & (1 << xkb->indices.super))
            seat->active_modifiers |= MOD_SUPER;
        if (mods_active & (1 << xkb->indices.shift))
            seat->active_modifiers |= MOD_SHIFT;
    }
}

void swc_seat_handle_button(struct swc_seat * seat, uint32_t time,
                            uint32_t button, uint32_t state)
{
}

/* Wayland Seat Interface */
void swc_seat_get_pointer(struct wl_client * client,
                          struct wl_resource * resource, uint32_t id)
{
    struct wl_resource * client_resource;
    struct swc_seat * seat = resource->data;
    struct wl_pointer * pointer = &seat->pointer;

    /* pointer interface? */
    client_resource = wl_client_add_object(client, &wl_pointer_interface,
                                           NULL, id, seat);
    client_resource->destroy = &swc_unbind_resource;

    wl_list_insert(&pointer->resource_list, &client_resource->link);

    if (pointer->focus && pointer->focus->resource.client == client)
    {
        //wl_pointer_set_focus(pointer, pointer->focus);
    }
}

void swc_seat_get_keyboard(struct wl_client * client,
                           struct wl_resource * resource, uint32_t id)
{
    struct wl_resource * client_resource;
    struct swc_seat * seat = resource->data;
    struct wl_keyboard * keyboard = &seat->keyboard;

    client_resource = wl_client_add_object(client, &wl_keyboard_interface,
                                           NULL, id, seat);
    client_resource->destroy = &swc_unbind_resource;

    wl_list_insert(&keyboard->resource_list, &client_resource->link);

    wl_keyboard_send_keymap(client_resource, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                            seat->xkb.keymap.fd, seat->xkb.keymap.size);

    if (keyboard->focus && keyboard->focus->resource.client == client)
        wl_keyboard_set_focus(keyboard, keyboard->focus);
}

void swc_seat_get_touch(struct wl_client * client,
                        struct wl_resource * resource, uint32_t id)
{
    /*
    struct wl_resource * client_resource;
    struct swc_seat * seat = resource->data;
    struct wl_touch * touch = &seat->touch;

    client_resource = wl_client_add_object(client, &wl_touch_interface,
                                           NULL, id, seat);
    client_resource->destroy = &swc_unbind_resource;

    wl_list_insert(&touch->resource_list, &client_resource->link);
    */
}

