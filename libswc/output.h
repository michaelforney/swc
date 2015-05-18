#ifndef SWC_OUTPUT_H
#define SWC_OUTPUT_H

#include <stdint.h>
#include <pixman.h>
#include <wayland-util.h>
#include <xf86drmMode.h>

struct wl_display;

struct output {
	struct screen *screen;

	/* The physical dimensions (in mm) of this output */
	uint32_t physical_width, physical_height;

	struct wl_array modes;
	struct mode *preferred_mode;

	pixman_region32_t current_damage, previous_damage;

	/* The DRM connector corresponding to this output */
	uint32_t connector;

	struct wl_global *global;
	struct wl_list resources;
	struct wl_list link;
};

struct output *output_new(drmModeConnector *connector);
void output_destroy(struct output *output);

#endif
