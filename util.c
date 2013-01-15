#include "util.h"

#include <stdlib.h>

void swc_unbind_resource(struct wl_resource * resource)
{
    wl_list_remove(&resource->link);
    free(resource);
}

