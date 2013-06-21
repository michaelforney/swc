#include "region.h"

#include <stdlib.h>

static void destroy(struct wl_client * client, struct wl_resource * resource)
{
    wl_resource_destroy(resource);
}

static void add(struct wl_client * client, struct wl_resource * resource,
                int32_t x, int32_t y, int32_t width, int32_t height)
{
    struct swc_region * region = wl_resource_get_user_data(resource);

    pixman_region32_union_rect(&region->region, &region->region,
                               x, y, width, height);
}

static void subtract(struct wl_client * client, struct wl_resource * resource,
                     int32_t x, int32_t y, int32_t width, int32_t height)
{
    struct swc_region * region = wl_resource_get_user_data(resource);
    pixman_region32_t operand;

    pixman_region32_init_rect(&operand, x, y, width, height);
    pixman_region32_subtract(&region->region, &region->region, &operand);
}

static const struct wl_region_interface region_implementation = {
    .destroy = &destroy,
    .add = &add,
    .subtract = &subtract
};

static void region_destroy(struct wl_resource * resource)
{
    struct swc_region * region = wl_resource_get_user_data(resource);

    /* Finish the region. */
    pixman_region32_fini(&region->region);

    free(region);
}

struct swc_region * swc_region_new(struct wl_client * client, uint32_t id)
{
    struct swc_region * region;

    region = malloc(sizeof *region);

    if (!region)
        return NULL;

    /* Initialize the region. */
    pixman_region32_init(&region->region);

    /* Add the region to the client. */
    region->resource = wl_client_add_object(client, &wl_region_interface,
                                            &region_implementation, id, region);
    wl_resource_set_destructor(region->resource, &region_destroy);

    return region;
}

