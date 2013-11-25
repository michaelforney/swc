#include "mode.h"

#include <stdio.h>

bool swc_mode_initialize(struct swc_mode * mode, drmModeModeInfo * mode_info)
{
    mode->width = mode_info->hdisplay;
    mode->height = mode_info->vdisplay;
    mode->refresh = mode_info->vrefresh * 1000;
    mode->preferred = mode_info->type & DRM_MODE_TYPE_PREFERRED;

    mode->info = *mode_info;

    return true;
}

void swc_mode_finish(struct swc_mode * mode)
{
}

