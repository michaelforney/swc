/* swc: drm.c
 *
 * Copyright (c) 2013, 2014 Michael Forney
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "drm.h"
#include "event.h"
#include "internal.h"
#include "launch.h"
#include "output.h"
#include "screen.h"
#include "util.h"
#include "wayland_buffer.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <drm.h>
#include <xf86drm.h>
#include <wld/wld.h>
#include <wld/drm.h>
#include <wayland-server.h>
#include "wayland-drm-server-protocol.h"

struct swc_drm swc_drm;

static struct {
	char *path;

	uint32_t taken_ids;

	struct wl_global *global;
	struct wl_event_source *event_source;
} drm;

static void
authenticate(struct wl_client *client, struct wl_resource *resource, uint32_t magic)
{
	wl_drm_send_authenticated(resource);
}

static void
create_buffer(struct wl_client *client, struct wl_resource *resource, uint32_t id,
              uint32_t name, int32_t width, int32_t height, uint32_t stride, uint32_t format)
{
	wl_resource_post_error(resource, WL_DRM_ERROR_INVALID_NAME, "GEM names are not supported, use a PRIME fd instead");
}

static void
create_planar_buffer(struct wl_client *client, struct wl_resource *resource, uint32_t id,
                     uint32_t name, int32_t width, int32_t height, uint32_t format,
                     int32_t offset0, int32_t stride0,
                     int32_t offset1, int32_t stride1,
                     int32_t offset2, int32_t stride2)
{
	wl_resource_post_error(resource, WL_DRM_ERROR_INVALID_FORMAT, "planar buffers are not supported\n");
}

static void
create_prime_buffer(struct wl_client *client, struct wl_resource *resource, uint32_t id,
                    int32_t fd, int32_t width, int32_t height, uint32_t format,
                    int32_t offset0, int32_t stride0,
                    int32_t offset1, int32_t stride1,
                    int32_t offset2, int32_t stride2)
{
	struct wld_buffer *buffer;
	struct wl_resource *buffer_resource;
	union wld_object object = { .i = fd };

	buffer = wld_import_buffer(swc.drm->context, WLD_DRM_OBJECT_PRIME_FD, object, width, height, format, stride0);
	close(fd);

	if (!buffer)
		goto error0;

	buffer_resource = wayland_buffer_create_resource(client, wl_resource_get_version(resource), id, buffer);

	if (!buffer_resource)
		goto error1;

	return;

error1:
	wld_buffer_unreference(buffer);
error0:
	wl_resource_post_no_memory(resource);
}

static const struct wl_drm_interface drm_implementation = {
	.authenticate = authenticate,
	.create_buffer = create_buffer,
	.create_planar_buffer = create_planar_buffer,
	.create_prime_buffer = create_prime_buffer,
};

static int
select_card(const struct dirent *entry)
{
	unsigned num;
	return sscanf(entry->d_name, "card%u", &num) == 1;
}

static bool
find_primary_drm_device(char *path, size_t size)
{
	struct dirent **cards, *card = NULL;
	int num_cards, ret;
	unsigned index;
	FILE *file;
	unsigned char boot_vga;

	num_cards = scandir("/dev/dri", &cards, &select_card, &alphasort);

	if (num_cards == -1)
		return false;

	for (index = 0; index < num_cards; ++index) {
		snprintf(path, size, "/sys/class/drm/%s/device/boot_vga", cards[index]->d_name);

		if ((file = fopen(path, "r"))) {
			ret = fscanf(file, "%hhu", &boot_vga);
			fclose(file);

			if (ret == 1 && boot_vga) {
				free(card);
				card = cards[index];
				DEBUG("/dev/dri/%s is the primary GPU\n", card->d_name);
				break;
			}
		}

		if (!card)
			card = cards[index];
		else
			free(cards[index]);
	}

	free(cards);

	if (!card)
		return false;

	if (snprintf(path, size, "/dev/dri/%s", card->d_name) >= size)
		return false;

	free(card);
	return true;
}

static bool
find_available_crtc(drmModeRes *resources, drmModeConnector *connector, uint32_t taken_crtcs, int *crtc_index)
{
	int i, j;
	uint32_t possible_crtcs;
	drmModeEncoder *encoder;

	for (i = 0; i < connector->count_encoders; ++i) {
		encoder = drmModeGetEncoder(swc.drm->fd, connector->encoders[i]);
		possible_crtcs = encoder->possible_crtcs;
		drmModeFreeEncoder(encoder);

		for (j = 0; j < resources->count_crtcs; ++j) {
			if ((possible_crtcs & (1 << j)) && !(taken_crtcs & (1 << j))) {
				*crtc_index = j;
				return true;
			}
		}
	}

	return false;
}

static bool
find_available_id(uint32_t *id)
{
	uint32_t index = __builtin_ffsl(~drm.taken_ids);

	if (index == 0)
		return false;

	*id = index - 1;
	return true;
}

static void
handle_vblank(int fd, unsigned int sequence, unsigned int sec, unsigned int usec, void *data)
{
}

static void
handle_page_flip(int fd, unsigned int sequence, unsigned int sec, unsigned int usec, void *data)
{
	struct drm_handler *handler = data;

	handler->page_flip(handler, sec * 1000 + usec / 1000);
}

static drmEventContext event_context = {
	.version = DRM_EVENT_CONTEXT_VERSION,
	.vblank_handler = handle_vblank,
	.page_flip_handler = handle_page_flip,
};

static int
handle_data(int fd, uint32_t mask, void *data)
{
	drmHandleEvent(fd, &event_context);
	return 1;
}

static void
bind_drm(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource;

	if (version > 2)
		version = 2;

	resource = wl_resource_create(client, &wl_drm_interface, version, id);
	wl_resource_set_implementation(resource, &drm_implementation, NULL, NULL);

	if (version >= 2)
		wl_drm_send_capabilities(resource, WL_DRM_CAPABILITY_PRIME);

	wl_drm_send_device(resource, drm.path);
	wl_drm_send_format(resource, WL_DRM_FORMAT_XRGB8888);
	wl_drm_send_format(resource, WL_DRM_FORMAT_ARGB8888);
}

bool
drm_initialize(void)
{
	char primary[128];

	if (!find_primary_drm_device(primary, sizeof(primary))) {
		ERROR("Could not find DRM device\n");
		goto error0;
	}

	drm.taken_ids = 0;
	swc.drm->fd = launch_open_device(primary, O_RDWR | O_CLOEXEC);
	if (swc.drm->fd == -1) {
		ERROR("Could not open DRM device at %s\n", primary);
		goto error0;
	}

	drm.path = drmGetRenderDeviceNameFromFd(swc.drm->fd);
	if (!drm.path) {
		ERROR("Could not determine render node path\n");
		goto error1;
	}

	if (!(swc.drm->context = wld_drm_create_context(swc.drm->fd))) {
		ERROR("Could not create WLD DRM context\n");
		goto error1;
	}

	if (!(swc.drm->renderer = wld_create_renderer(swc.drm->context))) {
		ERROR("Could not create WLD DRM renderer\n");
		goto error2;
	}

	drm.event_source = wl_event_loop_add_fd(swc.event_loop, swc.drm->fd, WL_EVENT_READABLE, &handle_data, NULL);

	if (!drm.event_source) {
		ERROR("Could not create DRM event source\n");
		goto error3;
	}

	if (!wld_drm_is_dumb(swc.drm->context)) {
		drm.global = wl_global_create(swc.display, &wl_drm_interface, 2, NULL, &bind_drm);

		if (!drm.global) {
			ERROR("Could not create wl_drm global\n");
			goto error4;
		}
	}

	return true;

error4:
	wl_event_source_remove(drm.event_source);
error3:
	wld_destroy_renderer(swc.drm->renderer);
error2:
	wld_destroy_context(swc.drm->context);
error1:
	close(swc.drm->fd);
error0:
	return false;
}

void
drm_finalize(void)
{
	if (drm.global)
		wl_global_destroy(drm.global);
	wl_event_source_remove(drm.event_source);
	wld_destroy_renderer(swc.drm->renderer);
	wld_destroy_context(swc.drm->context);
	free(drm.path);
	close(swc.drm->fd);
}

bool
drm_create_screens(struct wl_list *screens)
{
	drmModeRes *resources;
	drmModeConnector *connector;
	int i;
	struct output *output;
	uint32_t taken_crtcs = 0;

	if (!(resources = drmModeGetResources(swc.drm->fd))) {
		ERROR("Could not get DRM resources\n");
		return false;
	}

	for (i = 0; i < resources->count_connectors; ++i, drmModeFreeConnector(connector)) {
		connector = drmModeGetConnector(swc.drm->fd, resources->connectors[i]);

		if (connector->connection == DRM_MODE_CONNECTED) {
			int crtc_index;
			uint32_t id;

			if (!find_available_crtc(resources, connector, taken_crtcs, &crtc_index)) {
				WARNING("Could not find CRTC for connector %d\n", i);
				continue;
			}

			if (!find_available_id(&id)) {
				WARNING("No more available output IDs\n");
				drmModeFreeConnector(connector);
				break;
			}

			if (!(output = output_new(connector)))
				continue;

			output->screen = screen_new(resources->crtcs[crtc_index], output);
			output->screen->id = id;

			taken_crtcs |= 1 << crtc_index;
			drm.taken_ids |= 1 << id;

			wl_list_insert(screens, &output->screen->link);
		}
	}

	drmModeFreeResources(resources);

	return true;
}
