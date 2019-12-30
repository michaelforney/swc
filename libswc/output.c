#include "output.h"
#include "drm.h"
#include "internal.h"
#include "mode.h"
#include "screen.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <drm.h>
#include <xf86drm.h>

static void
bind_output(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct output *output = data;
	struct screen *screen = output->screen;
	struct mode *mode;
	struct wl_resource *resource;
	uint32_t flags;

	resource = wl_resource_create(client, &wl_output_interface, version, id);

	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, NULL, output, &remove_resource);
	wl_list_insert(&output->resources, wl_resource_get_link(resource));

	wl_output_send_geometry(resource, screen->base.geometry.x, screen->base.geometry.y,
	                        output->physical_width, output->physical_height,
	                        0, "unknown", "unknown", WL_OUTPUT_TRANSFORM_NORMAL);

	wl_array_for_each (mode, &output->modes) {
		flags = 0;
		if (mode->preferred)
			flags |= WL_OUTPUT_MODE_PREFERRED;
		if (mode_equal(&screen->planes.primary.mode, mode))
			flags |= WL_OUTPUT_MODE_CURRENT;

		wl_output_send_mode(resource, flags, mode->width, mode->height, mode->refresh);
	}

	if (version >= 2)
		wl_output_send_done(resource);
}

struct output *
output_new(drmModeConnectorPtr connector)
{
	struct output *output;
	struct mode *modes;
	uint32_t i;

	if (!(output = malloc(sizeof(*output)))) {
		ERROR("Failed to allocated output\n");
		goto error0;
	}

	output->global = wl_global_create(swc.display, &wl_output_interface, 2, output, &bind_output);

	if (!output->global) {
		ERROR("Failed to create output global\n");
		goto error1;
	}

	output->physical_width = connector->mmWidth;
	output->physical_height = connector->mmHeight;
	output->preferred_mode = NULL;

	wl_list_init(&output->resources);
	wl_array_init(&output->modes);
	pixman_region32_init(&output->current_damage);
	pixman_region32_init(&output->previous_damage);

	output->connector = connector->connector_id;

	if (connector->count_modes == 0)
		goto error2;

	modes = wl_array_add(&output->modes, connector->count_modes * sizeof(*modes));
	if (!modes)
		goto error2;

	for (i = 0; i < connector->count_modes; ++i) {
		mode_initialize(&modes[i], &connector->modes[i]);

		if (modes[i].preferred)
			output->preferred_mode = &modes[i];
	}

	if (!output->preferred_mode)
		output->preferred_mode = &modes[0];

	return output;

error2:
	wl_global_destroy(output->global);
error1:
	free(output);
error0:
	return NULL;
}

void
output_destroy(struct output *output)
{
	wl_array_release(&output->modes);
	wl_global_destroy(output->global);
	free(output);
}
