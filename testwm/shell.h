#ifndef TESTWM_SHELL_H
#define TESTWM_SHELL_H 1

#include <wayland-server.h>

struct shell
{
};

extern struct wl_shell_interface shell_implementation;

void shell_initialize(struct shell * shell, struct wl_display * display);

#endif

