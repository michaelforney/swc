/* swc: libswc/subsurface.c
 *
 * Copyright (c) 2015-2019 Michael Forney
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

#include "subsurface.h"
#include "util.h"

#include <stdlib.h>
#include <wayland-server.h>

static void
set_position(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y)
{
	/* TODO: Implement. */
}

static void
place_above(struct wl_client *client, struct wl_resource *resource, struct wl_resource *sibling_resource)
{
	/* TODO: Implement. */
}

static void
place_below(struct wl_client *client, struct wl_resource *resource, struct wl_resource *sibling_resource)
{
	/* TODO: Implement. */
}

static void
set_sync(struct wl_client *client, struct wl_resource *resource)
{
	/* TODO: Implement. */
}

static void
set_desync(struct wl_client *client, struct wl_resource *resource)
{
	/* TODO: Implement. */
}

static const struct wl_subsurface_interface subsurface_impl = {
	.destroy = destroy_resource,
	.set_position = set_position,
	.place_above = place_above,
	.place_below = place_below,
	.set_sync = set_sync,
	.set_desync = set_desync,
};

static void
subsurface_destroy(struct wl_resource *resource)
{
	struct subsurface *subsurface = wl_resource_get_user_data(resource);
	free(subsurface);
}

struct subsurface *
subsurface_new(struct wl_client *client, uint32_t version, uint32_t id)
{
	struct subsurface *subsurface;

	if (!(subsurface = malloc(sizeof(*subsurface))))
		goto error0;

	subsurface->resource = wl_resource_create(client, &wl_subsurface_interface, version, id);

	if (!subsurface->resource)
		goto error1;

	wl_resource_set_implementation(subsurface->resource, &subsurface_impl, subsurface, &subsurface_destroy);

	return subsurface;

error1:
	free(subsurface);
error0:
	return NULL;
}
