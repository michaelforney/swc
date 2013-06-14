#ifndef SWC_BUFFER_H
#define SWC_BUFFER_H 1

#include "drm.h"

#include <stdbool.h>
#include <pixman.h>

struct swc_buffer
{
    uint32_t id;

    drm_intel_bo * bo;

    uint32_t width, height, pitch;
};

bool swc_buffer_initialize(struct swc_buffer * buffer, struct swc_drm * drm,
                           uint32_t width, uint32_t height);

void swc_buffer_finish(struct swc_buffer * buffer, struct swc_drm * drm);

void swc_buffer_ref_image(struct swc_buffer * buffer);

void swc_buffer_unref_image(struct swc_buffer * buffer);

#endif

