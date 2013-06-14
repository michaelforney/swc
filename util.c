#include "util.h"

#include <stdlib.h>
#include <stdio.h>

void swc_unbind_resource(struct wl_resource * resource)
{
    wl_list_remove(&resource->link);
    free(resource);
}

struct wl_resource * swc_find_resource_for_client(struct wl_list * resources,
                                                  struct wl_client * client)
{
    struct wl_resource * resource;

    printf("looking for client %p\n", client);

    wl_list_for_each(resource, resources, link)
    {
        if (wl_resource_get_client(resource) == client)
        {
            printf("found client\n");
            return resource;
        }
    }

    printf("couldn't find resource\n");

    return NULL;
}

