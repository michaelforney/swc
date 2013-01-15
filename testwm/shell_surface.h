#ifndef TESTWM_SHELL_SURFACE_H
#define TESTWM_SHELL_SURFACE_H 1

struct shell_surface
{
    struct swc_surface * surface;
    struct wl_resource * resource;
};

extern struct wl_shell_surface_interface shell_surface_implementation;

#endif

