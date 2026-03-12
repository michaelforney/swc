/* swc: libswc/output.h
 *
 * Copyright (c) 2013-2015 Michael Forney
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

#ifndef SWC_OUTPUT_H
#define SWC_OUTPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pixman.h>
#include <stdint.h>
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

#ifdef __cplusplus
}
#endif

#endif /* SWC_OUTPUT_H */
