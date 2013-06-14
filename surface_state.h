#ifndef SWC_SURFACE_STATE_H
#define SWC_SURFACE_STATE_H 1

#include <libdrm/intel_bufmgr.h>

#include <wayland-server.h>
#include <pixman.h>

union swc_renderer_surface_state
{
    struct
    {
        pixman_image_t * image;
    } shm;
    struct
    {
        drm_intel_bo * bo;
        uint32_t pitch;
    } drm;
};

struct swc_compositor_surface_state
{
    struct swc_compositor * compositor;
    struct wl_listener event_listener;
};

#endif

