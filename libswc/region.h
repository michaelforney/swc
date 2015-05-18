#ifndef SWC_REGION_H
#define SWC_REGION_H

#include <stdbool.h>
#include <pixman.h>

struct wl_client;

struct region {
	struct wl_resource *resource;
	pixman_region32_t region;
};

struct region *region_new(struct wl_client *client, uint32_t version, uint32_t id);

#endif
