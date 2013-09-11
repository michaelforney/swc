#ifndef SWC_RENDERER_H
#define SWC_RENDERER_H 1

#include "output.h"
#include "surface.h"
#include "drm.h"

struct swc_render_target
{
    struct wld_drawable * drawable;
    pixman_rectangle32_t geometry;
};

struct swc_renderer
{
    struct swc_drm * drm;
    struct swc_render_target target;
};

bool swc_renderer_initialize(struct swc_renderer * renderer,
                             struct swc_drm * drm);

void swc_renderer_finalize(struct swc_renderer * renderer);

void swc_renderer_set_target(struct swc_renderer * renderer,
                             struct swc_plane * plane);

void swc_renderer_repaint(struct swc_renderer * renderer,
                          pixman_region32_t * damage,
                          pixman_region32_t * base_damage,
                          struct wl_list * surfaces);

void swc_renderer_attach(struct swc_renderer * renderer,
                         struct swc_surface * surface,
                         struct wl_resource * resource);

void swc_renderer_flush(struct swc_renderer * renderer,
                        struct swc_surface * surface);

#endif

