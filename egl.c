#include "egl.h"

#include <stdio.h>
#include <string.h>

#define EGL_EGLEXT_PROTOTYPES
#include <EGL/eglext.h>
#undef EGL_EGLEXT_PROTOTYPES

bool swc_egl_initialize(struct swc_egl * egl, struct gbm_device * gbm)
{
    const char * extensions;

    egl->display = eglGetDisplay(gbm);

    if (egl->display == NULL)
    {
        printf("could not create egl display\n");
        goto error_base;
    }

    if (!eglInitialize(egl->display, NULL, NULL))
    {
        printf("could not initialize egl display\n");
        goto error_base;
    }

#if EGL_WL_bind_wayland_display
    extensions = eglQueryString(egl->display, EGL_EXTENSIONS);

    if (!extensions)
    {
        printf("could not query EGL extensions\n");
        goto error_display;
    }

    if (strstr(extensions, "EGL_WL_bind_wayland_display"))
        egl->has_bind_wayland_display = true;
    else
    {
        printf("warning: headers claim EGL_WL_bind_wayland_display exists, "
               "but it is not in the queried extension list\n");
        egl->has_bind_wayland_display = false;
    }
#else
    printf("don't have EGL_WL_bind_wayland_display extension\n");
    egl->has_bind_wayland_display = false;
#endif

    return true;

  error_display:
    eglTerminate(egl->display);
    eglReleaseThread();
  error_base:
    return false;
}

void swc_egl_finish(struct swc_egl * egl)
{
    eglTerminate(egl->display);
    eglReleaseThread();
}

bool swc_egl_bind_display(struct swc_egl * egl, struct wl_display * display)
{
#if EGL_WL_bind_wayland_display
    if (egl->has_bind_wayland_display)
        return eglBindWaylandDisplayWL(egl->display, display);
#endif

    /* If we don't have this extension, just continue normally. */
    return true;
}

bool swc_egl_unbind_display(struct swc_egl * egl, struct wl_display * display)
{
#if EGL_WL_bind_wayland_display
    if (egl->has_bind_wayland_display)
        return eglUnbindWaylandDisplayWL(egl->display, display);
#endif

    /* If we don't have this extension, just continue normally. */
    return true;
}

