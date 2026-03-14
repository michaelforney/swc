/* swc: libswc/screencopy.c
 *
 * Copyright (c) 2026 sewn <sewn@disroot.org>
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

#include "screencopy.h"
#include "compositor.h"
#include "internal.h"
#include "output.h"
#include "seat.h"
#include "screen.h"
#include "pointer.h"
#include "shm.h"
#include "util.h"
#include "wayland_buffer.h"

#include "wlr-screencopy-unstable-v1-server-protocol.h"
#include <pixman.h>
#include <stdint.h>
#include <time.h>
#include <wld/wld.h>

extern struct swc swc;

static void
composite_cursor(struct wld_buffer *dst, struct screen *screen)
{
	struct pointer *pointer = swc.seat ? swc.seat->pointer : NULL;
	struct wld_buffer *cursor_buf;
	pixman_image_t *src_img, *dest_img;
	int32_t dst_x, dst_y;

	if (!pointer || !pointer->cursor.buffer || !pointer->cursor.view.buffer) {
		return;
	}

	if (!(pointer->cursor.view.screens & screen_mask(screen))) {
		return;
	}

	cursor_buf = pointer->cursor.buffer;

	if (!cursor_buf->map) {
		return;
	}

	dst_x = pointer->cursor.view.geometry.x - screen->base.geometry.x;
	dst_y = pointer->cursor.view.geometry.y - screen->base.geometry.y;

	src_img = pixman_image_create_bits(PIXMAN_x8r8g8b8, cursor_buf->width,
	                                   cursor_buf->height, cursor_buf->map,
	                                   cursor_buf->pitch);

	dest_img = pixman_image_create_bits(PIXMAN_x8r8g8b8, dst->width,
	                                    dst->height, dst->map, dst->pitch);

	pixman_image_composite(PIXMAN_OP_SRC, src_img, NULL, dest_img, 0, 0, 0, 0,
	                       dst_x, dst_y, cursor_buf->width, cursor_buf->height);

	pixman_image_unref(src_img);
	pixman_image_unref(dest_img);
	wld_unmap(cursor_buf);
}

static struct screencopy_frame *
create_frame(struct wl_client *client, struct wl_resource *resource,
                     uint32_t id, int32_t overlay_cursor,
                     struct wl_resource *output_resource)
{
	struct screencopy_frame *frame;
	struct output *output =
	    output_resource ? wl_resource_get_user_data(output_resource) : NULL;
	struct screen *screen;

	(void)client;

	screen = output ? output->screen
	                : wl_container_of(swc.screens.next, screen, link);

	if (!(frame = malloc(sizeof(*frame)))) {
		return NULL;
	}
	frame->resource =
	    wl_resource_create(client, &zwlr_screencopy_frame_v1_interface,
	                       wl_resource_get_version(resource), id);
	if (!frame->resource) {
		return NULL;
	}

	frame->screen = screen;
	
	return frame;
}

static void
frame_send(struct screencopy_frame *frame, struct wld_buffer *buffer)
{
	struct wld_buffer *dst_buf = NULL;
	struct timespec ts;

	if (frame->client_buffer) {
		dst_buf = wayland_buffer_get(frame->client_buffer);
		if (!dst_buf) {
			wl_resource_post_error(frame->resource,
			                       WL_DISPLAY_ERROR_INVALID_OBJECT,
			                       "buffer is not a wl_buffer");
			return;
		}
	} else {
		wl_resource_post_error(frame->resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
		                       "no buffer given");
		return;
	}

	if (frame->geom.width != (uint32_t)dst_buf->width ||
	    frame->geom.height != (uint32_t)dst_buf->height ||
	    frame->stride < (int32_t)dst_buf->pitch) {
		wl_resource_post_error(frame->resource,
		                       ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER,
		                       "buffer has invalid parameters");
		return;
	}
	
	if (!wld_set_target_buffer(swc.shm->renderer, dst_buf)) {
		 	zwlr_screencopy_frame_v1_send_failed(frame->resource);
		 	return;
	}
			
	wld_copy_rectangle(swc.shm->renderer, buffer,
	                   0, 0,
	                   frame->geom.x - frame->screen->base.geometry.x,
	                   frame->geom.y - frame->screen->base.geometry.y,
	                   buffer->width, buffer->height);

	/* XXX: Currently, cursor composition is very slow since the cursor buffer
   * is hardware backed. There should be a way to transform the cursor to
   * software. */
	if (frame->overlay_cursor)
		composite_cursor(dst_buf, frame->screen);
	
	zwlr_screencopy_frame_v1_send_flags(frame->resource, 0);
	clock_gettime(CLOCK_MONOTONIC, &ts);
	zwlr_screencopy_frame_v1_send_ready(frame->resource, (ts.tv_sec >> 32),
	                                    (ts.tv_sec & 0xFFFFFFFF), ts.tv_nsec);
}

static void
frame_screen_attach(struct wl_listener *listener, void *data)
{
	struct wld_buffer *buffer = data;
	struct screencopy_frame *frame =
	    wl_container_of(listener, frame, screen_attach);
	const struct swc_rectangle *geom = &frame->screen->base.geometry;
	struct pixman_region32 damage, screen_damage;
	struct pixman_box32 *box;

	if (frame->with_damage) {
		/* XXX: Untested, no modern recorder uses this. */
		pixman_region32_init(&damage);
		pixman_region32_init(&screen_damage);
		compositor_get_damage(&damage);
		pixman_region32_intersect_rect(&screen_damage, &damage, geom->x, geom->y, geom->width, geom->height);
		pixman_region32_translate(&damage, -geom->x, -geom->y);
		box = pixman_region32_extents(&screen_damage);
	
		zwlr_screencopy_frame_v1_send_damage(frame->resource, box->x1, box->y1, box->x2 - box->x1, box->y2 - box->y1);
	};

	wl_list_remove(&frame->screen_attach.link);
	frame_send(frame, buffer);
}

static void
frame_copy(struct wl_client *client, struct wl_resource *resource, struct wl_resource *buffer_resource)
{
	struct screencopy_frame *frame = wl_resource_get_user_data(resource);
	(void)client;

	frame->client_buffer = buffer_resource;
	wl_signal_add(&frame->screen->planes.primary.view.attach_signal, &frame->screen_attach);
	frame->screen_attach.notify = &frame_screen_attach;
	/* XXX: Ask for redraw instead of damaging all windows for screencopy. */
	compositor_damage_all();
}

static void
frame_copy_damage(struct wl_client *client, struct wl_resource *resource, struct wl_resource *buffer_resource)
{
	struct screencopy_frame *frame = wl_resource_get_user_data(resource);

	frame->with_damage = true;
	frame_copy(client, resource, buffer_resource);
}

static void
frame_destroy(struct wl_resource *resource)
{
	struct screencopy_frame *frame = wl_resource_get_user_data(resource);
	free(frame);
}

static const struct zwlr_screencopy_frame_v1_interface frame_impl = {
    .copy = frame_copy,
    .copy_with_damage = frame_copy_damage,
    .destroy = destroy_resource,
};

static void
capture(struct wl_client *client, struct wl_resource *resource, uint32_t id,
        int32_t overlay_cursor, struct wl_resource *output)
{
	struct screencopy_frame *frame;

	frame = create_frame(client, resource, id, overlay_cursor, output);
	if (!frame) {
		wl_resource_post_no_memory(resource);
		return;
	}

	frame->overlay_cursor = overlay_cursor;
	frame->geom = frame->screen->base.geometry;
	frame->stride = frame->geom.width * 4;
	wl_resource_set_implementation(frame->resource, &frame_impl, frame,
	                               &frame_destroy);

	zwlr_screencopy_frame_v1_send_buffer(
	    frame->resource, WL_SHM_FORMAT_XRGB8888, frame->geom.width,
	    frame->geom.height, frame->stride);
}

static void
capture_region(struct wl_client *client, struct wl_resource *resource,
               uint32_t id, int32_t overlay_cursor, struct wl_resource *output,
               int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct screencopy_frame *frame;

	frame = create_frame(client, resource, id, overlay_cursor, output);
	if (!frame) {
		wl_resource_post_no_memory(resource);
		return;
	}

	frame->geom.width = width;
	frame->geom.height = height;
	frame->geom.x = x;
	frame->geom.y = y;
	frame->stride = width * 4;
	wl_resource_set_implementation(frame->resource, &frame_impl, frame,
	                               &frame_destroy);
	zwlr_screencopy_frame_v1_send_flags(frame->resource, 0);
	zwlr_screencopy_frame_v1_send_buffer(
	    frame->resource, WL_SHM_FORMAT_XRGB8888, frame->geom.width,
	    frame->geom.height, frame->stride);
}

static const struct zwlr_screencopy_manager_v1_interface screencopy_manager_impl = {
    .destroy = destroy_resource,
    .capture_output = capture,
    .capture_output_region = capture_region,
};

static void
bind_screencopy_manager(struct wl_client *client, void *data, uint32_t version,
                        uint32_t id)
{
	struct wl_resource *resource;

	resource = wl_resource_create(client, &zwlr_screencopy_manager_v1_interface,
	                              version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &screencopy_manager_impl, NULL, NULL);
}

struct wl_global *
screencopy_manager_create(struct wl_display *display)
{
	return wl_global_create(display, &zwlr_screencopy_manager_v1_interface, 2,
	                        NULL, &bind_screencopy_manager);
}
