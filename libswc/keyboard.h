#ifndef SWC_KEYBOARD_H
#define SWC_KEYBOARD_H 1

#include "surface.h"
#include "input_focus.h"
#include "xkb.h"

#include <wayland-util.h>

struct swc_keyboard;
struct wl_client;
struct wl_resource;

struct swc_keyboard_handler
{
    bool (* key)(struct swc_keyboard * keyboard, uint32_t time,
                 uint32_t key, uint32_t state);
    bool (* modifiers)(struct swc_keyboard * keyboard, uint32_t serial,
                       uint32_t mods_depressed, uint32_t mods_latched,
                       uint32_t mods_locked, uint32_t group);
};

struct swc_keyboard
{
    struct swc_input_focus focus;
    struct swc_input_focus_handler focus_handler;

    struct swc_keyboard_handler * handler;

    struct swc_xkb xkb;
    struct wl_array keys;

    struct
    {
            uint32_t mods_depressed;
            uint32_t mods_latched;
            uint32_t mods_locked;
            uint32_t group;
    } modifiers;
};

bool swc_keyboard_initialize(struct swc_keyboard * keyboard);
void swc_keyboard_finish(struct swc_keyboard * keyboard);
void swc_keyboard_set_focus(struct swc_keyboard * keyboard,
                            struct swc_surface * surface);
struct wl_resource * swc_keyboard_bind(struct swc_keyboard * keyboard,
                                       struct wl_client * client, uint32_t id);
void swc_keyboard_handle_key(struct swc_keyboard * keyboard, uint32_t time,
                             uint32_t key, uint32_t state);

#endif

