/* swc: swc/binding.c
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

#include "swc.h"
#include "binding.h"
#include "keyboard.h"
#include "util.h"

#include <wayland-util.h>

struct binding
{
    uint32_t value;
    uint32_t modifiers;
    swc_binding_handler_t handler;
    void * data;
};

static struct wl_array key_bindings;

static bool handle_key(struct swc_keyboard * keyboard, uint32_t time,
                       uint32_t key, uint32_t state)
{
    struct binding * binding;
    char keysym_name[64];

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
    {
        xkb_keysym_t keysym;

        keysym = xkb_state_key_get_one_sym(keyboard->xkb.state, key + 8);

        wl_array_for_each(binding, &key_bindings)
        {
            if (binding->value == keysym)
            {
                xkb_mod_mask_t mod_mask;
                uint32_t modifiers = 0;
                mod_mask = xkb_state_serialize_mods(keyboard->xkb.state,
                                                    XKB_STATE_MODS_EFFECTIVE);
                mod_mask = xkb_state_mod_mask_remove_consumed(keyboard->xkb.state, key + 8,
                                                              mod_mask);

                if (mod_mask & (1 << keyboard->xkb.indices.ctrl))
                    modifiers |= SWC_MOD_CTRL;
                if (mod_mask & (1 << keyboard->xkb.indices.alt))
                    modifiers |= SWC_MOD_ALT;
                if (mod_mask & (1 << keyboard->xkb.indices.super))
                    modifiers |= SWC_MOD_LOGO;
                if (mod_mask & (1 << keyboard->xkb.indices.shift))
                    modifiers |= SWC_MOD_SHIFT;

                if (binding->modifiers == SWC_MOD_ANY
                    || binding->modifiers == modifiers)
                {
                    binding->handler(time, keysym, binding->data);
                    printf("\t-> handled\n");
                    return true;
                }
            }
        }
    }

    return false;
}

static const struct swc_keyboard_handler binding_handler = {
    .key = &handle_key,
};
const struct swc_keyboard_handler * swc_binding_handler = &binding_handler;

bool swc_bindings_initialize()
{
    wl_array_init(&key_bindings);

    return true;
}

void swc_bindings_finalize()
{
    wl_array_release(&key_bindings);
}

EXPORT
void swc_add_key_binding(uint32_t modifiers, uint32_t value,
                         swc_binding_handler_t handler, void * data)
{
    struct binding * binding;

    binding = wl_array_add(&key_bindings, sizeof *binding);
    binding->value = value;
    binding->modifiers = modifiers;
    binding->handler = handler;
    binding->data = data;
}

