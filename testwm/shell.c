#include "shell.h"

#include "shell_surface.h"

static void get_shell_surface(struct wl_client * client,
                              struct wl_resource * resource, uint32_t id,
                              struct wl_resource * surface)
{
    wl_client_add_object(client, &wl_shell_surface_interface,
                         &shell_surface_implementation, id, surface->data);
}

struct wl_shell_interface shell_implementation = {
    .get_shell_surface = &get_shell_surface
};

static void bind_shell(struct wl_client * client, void * data, uint32_t version,
                       uint32_t id)
{
    wl_client_add_object(client, &wl_shell_interface, &shell_implementation,
                         id, NULL);
}

void shell_initialize(struct shell * shell, struct wl_display * display)
{
    wl_display_add_global(display, &wl_shell_interface, &shell_implementation,
                          &bind_shell);
}

