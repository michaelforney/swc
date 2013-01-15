#ifndef SWC_RENDERER_H
#define SWC_RENDERER_H 1

#include "output.h"
#include "surface.h"
#include "drm.h"
#include "i915/batch.h"

struct swc_renderer
{
    struct swc_drm * drm;

    struct i915_batch batch;
};

bool swc_renderer_initialize(struct swc_renderer * renderer,
                             struct swc_drm * drm);

void swc_renderer_finalize(struct swc_renderer * renderer);

void swc_renderer_repaint_output(struct swc_renderer * renderer,
                                 struct swc_output * output,
                                 struct wl_list * surfaces);

void swc_renderer_attach(struct swc_renderer * renderer,
                         struct wl_list * outputs,
                         struct swc_surface * surface,
                         struct wl_buffer * buffer);

#endif

