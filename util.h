#ifndef SWC_UTIL_H
#define SWC_UTIL_H 1

#include <stdbool.h>
#include <wayland-server.h>

#ifndef offsetof
#   define offsetof __builtin_offsetof
#endif

#define swc_container_of(ptr, type, member) ({                              \
    const typeof(((type *) 0)->member) *__mptr = (ptr);                     \
    ((type *) ((uintptr_t) __mptr - offsetof(type, member)));               \
})

void swc_remove_resource(struct wl_resource * resource);

int swc_launch_open_input_device(int socket, const char * path, int flags);
bool swc_launch_drm_master(int socket, int fd, bool set);

#endif

