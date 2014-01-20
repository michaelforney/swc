#include "util.h"

#include <wayland-server.h>

pixman_box32_t infinite_extents = {
    .x1 = INT32_MIN, .y1 = INT32_MIN,
    .x2 = INT32_MAX, .y2 = INT32_MAX
};

void swc_remove_resource(struct wl_resource * resource)
{
    wl_list_remove(wl_resource_get_link(resource));
}

