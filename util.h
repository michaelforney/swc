#ifndef SWC_UTIL_H
#define SWC_UTIL_H 1

#include <stdbool.h>
#include <wayland-server.h>

void swc_remove_resource(struct wl_resource * resource);

int swc_launch_open_input_device(int socket, const char * path, int flags);
bool swc_launch_drm_master(int socket, int fd, bool set);

#endif

