#ifndef SWC_DRM_H
#define SWC_DRM_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-server.h>

enum swc_drm_event_type
{
    SWC_DRM_PAGE_FLIP
};

struct swc_drm_event_data
{
    uint32_t time;
    struct swc_output * output;
};

struct swc_drm
{
    int fd;
    struct wld_context * context;
    struct wld_renderer * renderer;
    struct wl_signal event_signal;
};

bool swc_drm_initialize(const char * seat);
void swc_drm_finalize();

bool swc_drm_create_screens(struct wl_list * screens);

#endif

