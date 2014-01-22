/* swc: libswc/keyboard.h
 *
 * Copyright (c) 2013 Michael Forney
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

#ifndef SWC_KEYBOARD_H
#define SWC_KEYBOARD_H

#include "input_focus.h"
#include "surface.h"
#include "xkb.h"

#include <wayland-util.h>

struct swc_keyboard;
struct wl_client;

struct swc_keyboard_modifier_state
{
    uint32_t depressed;
    uint32_t latched;
    uint32_t locked;
    uint32_t group;
};

struct swc_keyboard_handler
{
    bool (* key)(struct swc_keyboard * keyboard, uint32_t time,
                 uint32_t key, uint32_t state);
    bool (* modifiers)(struct swc_keyboard * keyboard,
                       const struct swc_keyboard_modifier_state * state);

    struct wl_array keys;
    struct wl_list link;
};

struct swc_keyboard
{
    struct swc_input_focus focus;
    struct swc_input_focus_handler focus_handler;
    struct swc_xkb xkb;

    struct wl_list handlers;
    struct swc_keyboard_handler client_handler;

    struct swc_keyboard_modifier_state modifier_state;
    uint32_t modifiers;
};

bool swc_keyboard_initialize(struct swc_keyboard * keyboard);
void swc_keyboard_finalize(struct swc_keyboard * keyboard);
void swc_keyboard_set_focus(struct swc_keyboard * keyboard,
                            struct swc_surface * surface);
struct wl_resource * swc_keyboard_bind(struct swc_keyboard * keyboard,
                                       struct wl_client * client, uint32_t id);
void swc_keyboard_handle_key(struct swc_keyboard * keyboard, uint32_t time,
                             uint32_t key, uint32_t state);

#endif

