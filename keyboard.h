#ifndef SWC_KEYBOARD_H
#define SWC_KEYBOARD_H 1

#include "surface.h"

#include <wayland-server.h>

struct swc_keyboard;

struct swc_keyboard_handler
{
    bool (* key)(struct swc_keyboard * keyboard, uint32_t time,
                 uint32_t key, uint32_t state);
    bool (* modifiers)(struct swc_keyboard * keyboyard, uint32_t serial,
                       uint32_t mods_depressed, uint32_t mods_latched,
                       uint32_t mods_locked, uint32_t group);
};

struct swc_keyboard
{
    struct wl_list resources;

    struct
    {
        struct swc_surface * surface;
        struct wl_resource * resource;
    } focus;

    //struct wl_listener focus_listener;
    //struct wl_signal focus_signal;

    struct swc_keyboard_handler * handler;

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

#endif

