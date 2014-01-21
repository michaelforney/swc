#ifndef SWC_COMPOSITOR_H
#define SWC_COMPOSITOR_H

#include "pointer.h"

struct swc_compositor
{
    struct swc_pointer_handler pointer_handler;
};

bool swc_compositor_initialize();
void swc_compositor_finalize();

bool swc_compositor_add_surface(struct swc_surface * surface);
bool swc_compositor_remove_surface(struct swc_surface * surface);

void swc_compositor_surface_show(struct swc_surface * surface);
void swc_compositor_surface_hide(struct swc_surface * surface);
void swc_compositor_surface_set_border_color(struct swc_surface * surface,
                                             uint32_t color);
void swc_compositor_surface_set_border_width(struct swc_surface * surface,
                                             uint32_t width);

#endif

