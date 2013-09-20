#ifndef SWC_REGION_H
#define SWC_REGION_H 1

#include <stdbool.h>
#include <pixman.h>

struct wl_client;
struct wl_resource;

struct swc_region
{
    struct wl_resource * resource;
    pixman_region32_t region;
};

struct swc_region * swc_region_new(struct wl_client * client, uint32_t id);

#endif

