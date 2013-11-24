#include "swc.h"
#include "keyboard.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

static void enter(struct swc_input_focus_handler * handler,
                  struct wl_resource * resource, struct swc_surface * surface)
{
    struct swc_keyboard * keyboard;
    struct wl_client * client;
    struct wl_display * display;
    uint32_t serial;

    keyboard = CONTAINER_OF(handler, typeof(*keyboard), focus_handler);
    client = wl_resource_get_client(resource);
    display = wl_client_get_display(client);
    serial = wl_display_next_serial(display);

    wl_keyboard_send_enter(resource, serial, surface->resource,
                           &keyboard->keys);
}

static void leave(struct swc_input_focus_handler * handler,
                  struct wl_resource * resource, struct swc_surface * surface)
{
    struct wl_client * client;
    struct wl_display * display;
    uint32_t serial;

    client = wl_resource_get_client(resource);
    display = wl_client_get_display(client);
    serial = wl_display_next_serial(display);

    wl_keyboard_send_leave(resource, serial, surface->resource);
}

bool swc_keyboard_initialize(struct swc_keyboard * keyboard)
{
    if (!swc_xkb_initialize(&keyboard->xkb))
    {
        printf("could not initialize XKB\n");
        goto error0;
    }

    if (!swc_input_focus_initialize(&keyboard->focus, &keyboard->focus_handler))
        goto error1;

    wl_array_init(&keyboard->keys);
    keyboard->modifiers = 0;
    keyboard->focus_handler.enter = &enter;
    keyboard->focus_handler.leave = &leave;

    swc_input_focus_initialize(&keyboard->focus, &keyboard->focus_handler);

    return true;

  error1:
    swc_xkb_finish(&keyboard->xkb);
  error0:
    return false;
}

void swc_keyboard_finish(struct swc_keyboard * keyboard)
{
    wl_array_release(&keyboard->keys);
    swc_input_focus_finish(&keyboard->focus);
    swc_xkb_finish(&keyboard->xkb);
}

/**
 * Sets the focus of the keyboard to the specified surface.
 */
void swc_keyboard_set_focus(struct swc_keyboard * keyboard,
                            struct swc_surface * surface)
{
    swc_input_focus_set(&keyboard->focus, surface);
}

static void unbind(struct wl_resource * resource)
{
    struct swc_keyboard * keyboard = wl_resource_get_user_data(resource);

    swc_input_focus_remove_resource(&keyboard->focus, resource);
}

struct wl_resource * swc_keyboard_bind(struct swc_keyboard * keyboard,
                                       struct wl_client * client, uint32_t id)
{
    struct wl_resource * client_resource;

    client_resource = wl_resource_create(client, &wl_keyboard_interface, 1, id);
    wl_resource_set_implementation(client_resource, NULL, keyboard, &unbind);
    swc_input_focus_add_resource(&keyboard->focus, client_resource);

    /* Subtract one to remove terminating NULL character. */
    wl_keyboard_send_keymap(client_resource, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                            keyboard->xkb.keymap.fd,
                            keyboard->xkb.keymap.size - 1);

    return client_resource;
}

void swc_keyboard_handle_key(struct swc_keyboard * keyboard, uint32_t time,
                             uint32_t key, uint32_t state)
{
    uint32_t * pressed_key;
    uint32_t mods_depressed, mods_latched, mods_locked, mods_active, group;
    struct wl_display * display;
    uint32_t serial;
    enum xkb_key_direction direction;
    struct swc_xkb * xkb = &keyboard->xkb;

    if (keyboard->focus.resource)
    {
        struct wl_client * client
            = wl_resource_get_client(keyboard->focus.resource);
        display = wl_client_get_display(client);
    }

    /* Update keyboard state state */
    wl_array_for_each(pressed_key, &keyboard->keys)
    {
        if (*pressed_key == key)
        {
            /* Ignore repeat events. */
            if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
                return;
            else
            {
                /* Remove the key from the array */
                swc_array_remove(&keyboard->keys, pressed_key, sizeof key);
                break;
            }
        }
    }

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
    {
        pressed_key = wl_array_add(&keyboard->keys, sizeof key);
        *pressed_key = key;
    }

    /* Check if the key event is handled by the keyboard handler. */
    if ((!keyboard->handler || !keyboard->handler->key
         || !keyboard->handler->key(keyboard, time, key, state))
        && keyboard->focus.resource)
    {
        serial = wl_display_next_serial(display);
        wl_keyboard_send_key(keyboard->focus.resource, serial, time,
                             key, state);
    }

    /* Update XKB state. Apparently the keycodes are offset by 8 in XKB. */
    direction = state == WL_KEYBOARD_KEY_STATE_PRESSED ? XKB_KEY_DOWN
                                                       : XKB_KEY_UP;
    xkb_state_update_key(xkb->state, key + 8, direction);

    mods_depressed = xkb_state_serialize_mods(xkb->state, XKB_STATE_DEPRESSED);
    mods_latched = xkb_state_serialize_mods(xkb->state, XKB_STATE_LATCHED);
    mods_locked = xkb_state_serialize_mods(xkb->state, XKB_STATE_LOCKED);
    mods_active = mods_depressed | mods_latched;
    group = xkb_state_serialize_layout(xkb->state, XKB_STATE_LAYOUT_EFFECTIVE);

    if (mods_depressed != keyboard->mods_depressed
        || mods_latched != keyboard->mods_latched
        || mods_locked != keyboard->mods_locked
        || group != keyboard->group)
    {
        if (keyboard->focus.resource)
        {
            serial = wl_display_next_serial(display);
            wl_keyboard_send_modifiers(keyboard->focus.resource,
                                       serial, mods_depressed, mods_latched,
                                       mods_locked, group);
        }
    }

    keyboard->modifiers = 0;
    if (mods_active & (1 << keyboard->xkb.indices.ctrl))
        keyboard->modifiers |= SWC_MOD_CTRL;
    if (mods_active & (1 << keyboard->xkb.indices.alt))
        keyboard->modifiers |= SWC_MOD_ALT;
    if (mods_active & (1 << keyboard->xkb.indices.super))
        keyboard->modifiers |= SWC_MOD_LOGO;
    if (mods_active & (1 << keyboard->xkb.indices.shift))
        keyboard->modifiers |= SWC_MOD_SHIFT;

    keyboard->mods_depressed = mods_depressed;
    keyboard->mods_latched = mods_latched;
    keyboard->mods_locked = mods_locked;
    keyboard->group = group;
}

