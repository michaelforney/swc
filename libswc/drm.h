#ifndef SWC_DRM_H
#define SWC_DRM_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-server.h>

struct drm_handler {
	void (*page_flip)(struct drm_handler *handler, uint32_t time);
};

struct swc_drm {
	int fd;
	struct wld_context *context;
	struct wld_renderer *renderer;
};

bool drm_initialize(void);
void drm_finalize(void);

bool drm_create_screens(struct wl_list *screens);

#endif
