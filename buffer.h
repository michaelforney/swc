#ifndef SWC_BUFFER_H
#define SWC_BUFFER_H 1

#include "drm.h"
#include "i915/bo.h"

#include <stdbool.h>
#include <pixman.h>

struct swc_buffer
{
    uint32_t id;

    struct i915_bo bo;

    /* Pixman image using the mapped buffer for use with SHM. */
    pixman_image_t * image;

    uint32_t width, height, pitch;
};

bool swc_buffer_initialize(struct swc_buffer * buffer, struct swc_drm * drm,
                           uint32_t width, uint32_t height);

void swc_buffer_finish(struct swc_buffer * buffer, struct swc_drm * drm);

void swc_buffer_ref_image(struct swc_buffer * buffer, struct swc_drm * drm);

void swc_buffer_unref_image(struct swc_buffer * buffer);

#endif

