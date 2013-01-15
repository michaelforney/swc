#include "shell_surface.h"

#include <stdlib.h>
#include <wayland-server.h>

static void pong(struct wl_client * client, struct wl_resource * resource,
                 uint32_t serial)
{
}

static void move(struct wl_client * client, struct wl_resource * resource,
                 struct wl_resource * seat_resource, uint32_t serial)
{
}

static void resize(struct wl_client * client, struct wl_resource * resource,
                   struct wl_resource * seat_resource, uint32_t serial,
                   uint32_t edges)
{
}

static void set_toplevel(struct wl_client * client,
                         struct wl_resource * resource)
{
}

static void set_transient(struct wl_client * client,
                          struct wl_resource * resource,
                          struct wl_resource * parent_resource,
                          int32_t x, int32_t y, uint32_t flags)
{
}

static void set_fullscreen(struct wl_client * client,
                           struct wl_resource * resource, uint32_t method,
                           uint32_t framerate, struct wl_resource * output)
{
}

static void set_popup(struct wl_client * client, struct wl_resource * resource,
                      struct wl_resource * seat_resource, uint32_t serial,
                      struct wl_resource * parent_resource,
                      int32_t x, int32_t y, uint32_t flags)
{
}

static void set_maximized(struct wl_client * client,
                          struct wl_resource * resource,
                          struct wl_resource * output)
{
}

static void set_class(struct wl_client * client, struct wl_resource * resource,
                      const char * class_)
{
}

struct wl_shell_surface_interface shell_surface_implementation = {
    .pong = &pong,
    .move = &move,
    .resize = &resize,
    .set_toplevel = &set_toplevel,
    .set_transient = &set_transient,
    .set_fullscreen = &set_fullscreen,
    .set_popup = &set_popup,
    .set_maximized = &set_maximized,
    .set_class = &set_class
};

struct shell_surface * shell_surface_create(struct swc_surface * surface)
{
    struct shell_surface * shell_surface;

    shell_surface = malloc(sizeof *shell_surface);
    shell_surface->surface = surface;

    return shell_surface;
}

