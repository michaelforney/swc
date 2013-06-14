#ifndef SWC_UTIL_H
#define SWC_UTIL_H 1

#include <wayland-server.h>

/*
void swc_object_init(struct wl_object * object,
                     const struct wl_interface * interface,
                     const void * implementation, uint32_t id);

void swc_resource_init(struct wl_resource * resource,
                       struct wl_client * client, 

void swc_client_add_object(struct wl_clientstruct wl_resource * resource, );
*/

void swc_unbind_resource(struct wl_resource * resource);

struct wl_resource * swc_find_resource_for_client(struct wl_list * resources,
                                                  struct wl_client * client);

#endif

