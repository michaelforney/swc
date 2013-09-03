#ifndef SWC_EGL_H
#define SWC_EGL_H 1

#include <stdlib.h>
#include <stdbool.h>
#include <wayland-server.h>
#include <gbm.h>
#include <EGL/egl.h>

struct swc_egl
{
    EGLDisplay display;
    bool has_bind_wayland_display;
};

bool swc_egl_initialize(struct swc_egl * egl, struct gbm_device * gbm);

void swc_egl_finish(struct swc_egl * egl);

bool swc_egl_bind_display(struct swc_egl * egl, struct wl_display * display);

bool swc_egl_unbind_display(struct swc_egl * egl, struct wl_display * display);

#endif

