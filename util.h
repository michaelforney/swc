#ifndef SWC_UTIL_H
#define SWC_UTIL_H 1

#include <wayland-server.h>

void swc_unbind_resource(struct wl_resource * resource);

struct wl_resource * swc_find_resource_for_client(struct wl_list * resources,
                                                  struct wl_client * client);

#endif

