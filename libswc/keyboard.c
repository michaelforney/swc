/* swc: libswc/keyboard.c
 *
 * Copyright (c) 2013 Michael Forney
 *
 * Based in part upon input.c from weston, which is:
 *
 *     Copyright © 2013 Intel Corporation
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

#include "swc.h"
#include "compositor.h"
#include "internal.h"
#include "keyboard.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

static void enter(struct input_focus_handler * handler,
                  struct wl_resource * resource, struct compositor_view * view)
{
    struct keyboard * keyboard
        = CONTAINER_OF(handler, typeof(*keyboard), focus_handler);
    uint32_t serial;

    serial = wl_display_next_serial(swc.display);
    wl_keyboard_send_enter(resource, serial, view->surface->resource,
                           &keyboard->client_keys);
}

static void leave(struct input_focus_handler * handler,
                  struct wl_resource * resource, struct compositor_view * view)
{
    uint32_t serial;

    serial = wl_display_next_serial(swc.display);
    wl_keyboard_send_leave(resource, serial, view->surface->resource);
}

static bool client_handle_key(struct keyboard * keyboard, uint32_t time,
                              struct press * press, uint32_t state)
{
    uint32_t * client_key;

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
    {
        client_key = wl_array_add(&keyboard->client_keys, sizeof *client_key);

        if (!client_key)
            return false;

        *client_key = press->value;
    }
    else
    {
        wl_array_for_each(client_key, &keyboard->client_keys)
        {
            if (*client_key == press->value)
            {
                swc_array_remove(&keyboard->client_keys,
                                 client_key, sizeof *client_key);
                break;
            }
        }
    }

    if (keyboard->focus.resource)
    {
        wl_keyboard_send_key(keyboard->focus.resource, press->serial, time,
                             press->value, state);
    }

    return true;
}

static bool client_handle_modifiers
    (struct keyboard * keyboard, const struct keyboard_modifier_state * state)
{
    struct wl_client * client;
    struct wl_display * display;
    uint32_t serial;

    if (!keyboard->focus.resource)
        return false;

    client = wl_resource_get_client(keyboard->focus.resource);
    display = wl_client_get_display(client);
    serial = wl_display_next_serial(display);
    wl_keyboard_send_modifiers(keyboard->focus.resource, serial,
                               state->depressed, state->locked, state->latched,
                               state->group);

    return true;
}

bool keyboard_initialize(struct keyboard * keyboard)
{
    if (!swc_xkb_initialize(&keyboard->xkb))
    {
        ERROR("Could not initialize XKB\n");
        goto error0;
    }

    if (!input_focus_initialize(&keyboard->focus, &keyboard->focus_handler))
        goto error1;

    keyboard->modifier_state = (struct keyboard_modifier_state) { };
    keyboard->modifiers = 0;
    keyboard->focus_handler.enter = &enter;
    keyboard->focus_handler.leave = &leave;
    keyboard->client_handler.key = &client_handle_key;
    keyboard->client_handler.modifiers = &client_handle_modifiers;
    wl_array_init(&keyboard->client_keys);
    wl_array_init(&keyboard->keys);
    wl_list_init(&keyboard->handlers);
    wl_list_insert(&keyboard->handlers, &keyboard->client_handler.link);

    return true;

  error1:
    swc_xkb_finalize(&keyboard->xkb);
  error0:
    return false;
}

void keyboard_finalize(struct keyboard * keyboard)
{
    wl_array_release(&keyboard->client_keys);
    wl_array_release(&keyboard->keys);
    input_focus_finalize(&keyboard->focus);
    swc_xkb_finalize(&keyboard->xkb);
}

/**
 * Sets the focus of the keyboard to the specified surface.
 */
void keyboard_set_focus(struct keyboard * keyboard,
                        struct compositor_view * view)
{
    input_focus_set(&keyboard->focus, view);
}

static void unbind(struct wl_resource * resource)
{
    struct keyboard * keyboard = wl_resource_get_user_data(resource);

    input_focus_remove_resource(&keyboard->focus, resource);
}

struct wl_resource * keyboard_bind(struct keyboard * keyboard,
                                   struct wl_client * client, uint32_t id)
{
    struct wl_resource * client_resource;

    client_resource = wl_resource_create(client, &wl_keyboard_interface, 1, id);
    wl_resource_set_implementation(client_resource, NULL, keyboard, &unbind);
    input_focus_add_resource(&keyboard->focus, client_resource);

    /* Subtract one to remove terminating NULL character. */
    wl_keyboard_send_keymap(client_resource, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                            keyboard->xkb.keymap.fd,
                            keyboard->xkb.keymap.size - 1);

    return client_resource;
}

void keyboard_handle_key(struct keyboard * keyboard, uint32_t time,
                         uint32_t value, uint32_t state)
{
    struct key * key;
    struct keyboard_modifier_state modifier_state;
    enum xkb_key_direction direction;
    struct swc_xkb * xkb = &keyboard->xkb;
    struct keyboard_handler * handler;
    uint32_t serial;

    serial = wl_display_next_serial(swc.display);

    /* First handle key release events associated with a particular handler. */
    wl_array_for_each(key, &keyboard->keys)
    {
        if (key->press.value == value)
        {
            /* Ignore repeat events. */
            if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
                return;

            if (key->handler)
            {
                key->press.serial = serial;
                key->handler->key(keyboard, time, &key->press, state);
            }

            swc_array_remove(&keyboard->keys, key, sizeof *key);
            goto update_xkb_state;
        }
    }

    key = wl_array_add(&keyboard->keys, sizeof *key);

    if (!key)
        goto update_xkb_state;

    key->press.value = value;
    key->press.serial = serial;
    key->handler = NULL;

    /* Go through handlers to see if any will accept this key event. */
    wl_list_for_each(handler, &keyboard->handlers, link)
    {
        if (handler->key && handler->key(keyboard, time, &key->press, state))
        {
            key->handler = handler;
            break;
        }
    }

    /* Update XKB state. */
  update_xkb_state:
    direction = state == WL_KEYBOARD_KEY_STATE_PRESSED ? XKB_KEY_DOWN
                                                       : XKB_KEY_UP;
    xkb_state_update_key(xkb->state, XKB_KEY(value), direction);

    modifier_state.depressed
        = xkb_state_serialize_mods(xkb->state, XKB_STATE_DEPRESSED);
    modifier_state.latched
        = xkb_state_serialize_mods(xkb->state, XKB_STATE_LATCHED);
    modifier_state.locked
        = xkb_state_serialize_mods(xkb->state, XKB_STATE_LOCKED);
    modifier_state.group
        = xkb_state_serialize_layout(xkb->state, XKB_STATE_LAYOUT_EFFECTIVE);

    if (modifier_state.depressed != keyboard->modifier_state.depressed
        || modifier_state.latched != keyboard->modifier_state.latched
        || modifier_state.locked != keyboard->modifier_state.locked
        || modifier_state.group != keyboard->modifier_state.group)
    {
        uint32_t mods_active = modifier_state.depressed
                             | modifier_state.latched;

        /* Update keyboard modifier state. */
        keyboard->modifier_state = modifier_state;
        keyboard->modifiers = 0;
        if (mods_active & (1 << keyboard->xkb.indices.ctrl))
            keyboard->modifiers |= SWC_MOD_CTRL;
        if (mods_active & (1 << keyboard->xkb.indices.alt))
            keyboard->modifiers |= SWC_MOD_ALT;
        if (mods_active & (1 << keyboard->xkb.indices.super))
            keyboard->modifiers |= SWC_MOD_LOGO;
        if (mods_active & (1 << keyboard->xkb.indices.shift))
            keyboard->modifiers |= SWC_MOD_SHIFT;

        /* Run any modifier handlers. */
        wl_list_for_each(handler, &keyboard->handlers, link)
        {
            if (handler->modifiers)
                handler->modifiers(keyboard, &modifier_state);
        }
    }
}

