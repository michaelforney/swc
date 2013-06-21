#ifndef SWC_INPUT_H
#define SWC_INPUT_H 1

#include <stdbool.h>
#include <wayland-server.h>

struct swc_surface;

struct swc_input_handler
{
    void (* enter)(struct swc_input_handler * handler,
                   struct wl_resource * resource,
                   struct swc_surface * surface);
    void (* leave)(struct swc_input_handler * handler,
                   struct wl_resource * resource,
                   struct swc_surface * surface);
};

struct swc_input
{
    struct
    {
        struct wl_resource * resource;
        struct swc_surface * surface;
    } focus;

    struct swc_input_handler * handler;
    struct wl_list resources;
};

bool swc_input_initialize(struct swc_input * input,
                          struct swc_input_handler * input_handler);

void swc_input_finish(struct swc_input * input);

void swc_input_add_resource(struct swc_input * input,
                            struct wl_resource * resource);

void swc_input_remove_resource(struct swc_input * input,
                               struct wl_resource * resource);

void swc_input_set_focus(struct swc_input * input,
                         struct swc_surface * surface);

#endif

