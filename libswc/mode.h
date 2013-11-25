#ifndef SWC_MODE_H
#define SWC_MODE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <xf86drmMode.h>

#include <wayland-util.h>

struct swc_mode
{
    uint16_t width, height;
    uint32_t refresh;

    bool preferred;

    drmModeModeInfo info;
};

bool swc_mode_initialize(struct swc_mode * mode, drmModeModeInfo * mode_info);

void swc_mode_finish(struct swc_mode * mode);

#endif

