#ifndef SWC_DRM_H
#define SWC_DRM_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-server.h>

struct swc_drm_handler
{
    void (* page_flip)(struct swc_drm_handler * handler, uint32_t time);
};

struct swc_drm
{
    int fd;
    struct wld_context * context;
    struct wld_renderer * renderer;
};

bool swc_drm_initialize();
void swc_drm_finalize();

bool swc_drm_create_screens(struct wl_list * screens);

#endif

