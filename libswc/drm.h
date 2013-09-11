#ifndef SWC_DRM_H
#define SWC_DRM_H 1

#include <stdbool.h>
#include <stdint.h>
#include <libudev.h>
#include <gbm.h>
#include <wayland-server.h>
#include <libdrm/intel_bufmgr.h>

struct wld_drm_context * context;

enum swc_drm_event
{
    SWC_DRM_PAGE_FLIP
};

struct swc_drm
{
    int fd;
    uint32_t id;
    char * path;

    drm_intel_bufmgr * bufmgr;
    struct wld_drm_context * context;

    uint32_t taken_output_ids;

    struct wl_event_source * source;

    struct wl_signal event_signal;
};

bool swc_drm_initialize(struct swc_drm * drm, struct udev * udev,
                        const char * seat);

void swc_drm_finish(struct swc_drm * drm);

void swc_drm_add_event_sources(struct swc_drm * drm,
                               struct wl_event_loop * event_loop);

void swc_drm_add_globals(struct swc_drm * drm, struct wl_display * display);

void swc_drm_set_master(struct swc_drm * drm);

void swc_drm_drop_master(struct swc_drm * drm);

struct wl_list * swc_drm_create_outputs(struct swc_drm * drm);

#endif

