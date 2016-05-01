/* swc: libswc/primary_plane.h
 *
 * Copyright (c) 2013, 2016 Michael Forney
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

#ifndef SWC_PRIMARY_PLANE_H
#define SWC_PRIMARY_PLANE_H

#include "drm.h"
#include "mode.h"
#include "view.h"

#include <stdint.h>
#include <stdbool.h>
#include <wayland-server.h>

struct primary_plane {
	uint32_t crtc;
	drmModeCrtcPtr original_crtc_state;
	struct mode mode;
	struct view view;
	struct wl_array connectors;
	bool need_modeset;
	struct drm_handler drm_handler;
	struct wl_listener swc_listener;
};

bool primary_plane_initialize(struct primary_plane *plane, uint32_t crtc, struct mode *mode, uint32_t *connectors, uint32_t num_connectors);
void primary_plane_finalize(struct primary_plane *plane);

#endif
