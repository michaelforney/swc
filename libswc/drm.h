/* swc: libswc/drm.h
 *
 * Copyright (c) 2013-2015, 2018, 2019 Michael Forney
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

#ifndef SWC_DRM_H
#define SWC_DRM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

struct wl_list;
struct wld_buffer;

struct drm_handler {
	void (*page_flip)(struct drm_handler *handler, uint32_t time);
};

struct swc_drm {
	int fd;
	uint32_t cursor_w, cursor_h;
	struct wld_context *context;
	struct wld_renderer *renderer;
};

bool drm_initialize(void);
void drm_finalize(void);

bool drm_create_screens(struct wl_list *screens);
uint32_t drm_get_framebuffer(struct wld_buffer *buffer);

#ifdef __cplusplus
}
#endif

#endif /* SWC_DRM_H */
