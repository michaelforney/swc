#ifndef SWC_INTEL_BLT_H
#define SWC_INTEL_BLT_H 1

#include "batch.h"

#include <i915_drm.h>

#define COMMAND_TYPE_2D 0x2

#define BLT_OPCODE_XY_COLOR_BLT     0x50
#define BLT_OPCODE_XY_SRC_COPY_BLT  0x53

#define BR00_32BPP_BYTE_MASK_ALPHA (1 << 0)
#define BR00_32BPP_BYTE_MASK_COLOR (1 << 1)

static inline uint32_t br00(uint8_t client, uint8_t opcode,
                            uint8_t mask_32bpp,
                            bool src_tiling_enable, bool dst_tiling_enable,
                            uint8_t dword_length)
{
    return client           << 29   /* 31:29 */
        | opcode            << 22   /* 28:22 */
        | mask_32bpp        << 20   /* 21:20 */
                                    /* 19:16 */
        | src_tiling_enable << 15   /* 15 */
                                    /* 14:12 */
        | dst_tiling_enable << 11   /* 11 */
                                    /* 10:8 */
        | dword_length      << 0    /* 7:0 */
        ;
}

static inline uint32_t br09(uint32_t destination_address)
{
                                    /* 31:29 */
    return destination_address << 0 /* 28:0 */
        ;
}

static inline uint32_t br11(uint16_t source_pitch)
{
                                /* 31:16 */
    return source_pitch << 0    /* 15:0 */
        ;
}

static inline uint32_t br12(uint32_t source_address)
{
                                /* 31:29 */
    return source_address << 0  /* 28:0 */
        ;
}

#define BR13_COLOR_DEPTH_8BIT       0x0
#define BR13_COLOR_DEPTH_16BIT_565  0x1
#define BR13_COLOR_DEPTH_16BIT_1555 0x2
#define BR13_COLOR_DEPTH_32BIT      0x3

/* Commonly used raster operations */
#define BR13_RASTER_OPERATION_SOURCE    0xcc
#define BR13_RASTER_OPERATION_PATTERN   0xf0

static inline uint32_t br13(bool clipping_enable, uint8_t color_depth,
                            uint8_t raster_operation,
                            uint16_t destination_pitch)
{
                                    /* 31 */
    return clipping_enable  << 30   /* 30 */
                                    /* 29:26 */
        | color_depth       << 24   /* 25:24 */
        | raster_operation  << 16   /* 23:16 */
        | destination_pitch << 0    /* 15:0 */
        ;
}

static inline uint32_t br16(uint32_t color)
{
    return color << 0 /* 31:0 */
        ;
}

static inline uint32_t br22(uint16_t destination_y1, uint16_t destination_x1)
{
    return destination_y1   << 16   /* 31:16 */
        | destination_x1    << 0    /* 15:0 */
        ;
}

static inline uint32_t br23(uint16_t destination_y2, uint16_t destination_x2)
{
    return destination_y2   << 16   /* 31:16 */
        | destination_x2    << 0    /* 15:0 */
        ;
}

static inline uint32_t br26(uint16_t source_y1, uint16_t source_x1)
{
    return source_y1 << 16  /* 31:16 */
        | source_x1 << 16   /* 15:0 */
        ;
};

static inline void xy_src_copy_blt(struct intel_batch * batch,
                                   drm_intel_bo * src, uint16_t src_pitch,
                                   uint16_t src_x, uint16_t src_y,
                                   drm_intel_bo * dst, uint16_t dst_pitch,
                                   uint16_t dst_x, uint16_t dst_y,
                                   uint16_t width, uint16_t height)
{
    uint32_t src_tiling_mode, dst_tiling_mode, swizzle;

    intel_batch_ensure_space(batch, 8);

    drm_intel_bo_get_tiling(dst, &dst_tiling_mode, &swizzle);
    drm_intel_bo_get_tiling(src, &src_tiling_mode, &swizzle);

    drm_intel_bo_emit_reloc_fence
        (batch->bo, intel_batch_offset(batch, 4), dst, 0,
         I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER);
    drm_intel_bo_emit_reloc_fence
        (batch->bo, intel_batch_offset(batch, 7), src, 0,
         I915_GEM_DOMAIN_RENDER, 0);

    intel_batch_add_dwords(batch, 8,
        br00(COMMAND_TYPE_2D, BLT_OPCODE_XY_SRC_COPY_BLT,
             BR00_32BPP_BYTE_MASK_ALPHA | BR00_32BPP_BYTE_MASK_COLOR,
             src_tiling_mode != I915_TILING_NONE,
             dst_tiling_mode != I915_TILING_NONE, 6),
        br13(false, BR13_COLOR_DEPTH_32BIT, BR13_RASTER_OPERATION_SOURCE,
             dst_pitch >> 2),
        br22(dst_y, dst_x),
        br23(dst_y + height, dst_x + width),
        br09(dst->offset),
        br26(src_y, src_x),
        br11(src_pitch >> 2),
        br12(src->offset)
    );
}

static inline void xy_color_blt(struct intel_batch * batch,
                                drm_intel_bo * dst, uint16_t dst_pitch,
                                uint16_t dst_x1, uint16_t dst_y1,
                                uint16_t dst_x2, uint16_t dst_y2,
                                uint32_t color)
{
    uint32_t tiling_mode, swizzle_mode;

    intel_batch_ensure_space(batch, 6);

    drm_intel_bo_get_tiling(dst, &tiling_mode, &swizzle_mode);

    drm_intel_bo_emit_reloc_fence
        (batch->bo, intel_batch_offset(batch, 4), dst, 0,
         I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER);

    intel_batch_add_dwords(batch, 6,
        br00(COMMAND_TYPE_2D, BLT_OPCODE_XY_COLOR_BLT,
             BR00_32BPP_BYTE_MASK_ALPHA | BR00_32BPP_BYTE_MASK_COLOR,
             false, tiling_mode != I915_TILING_NONE, 4),
        br13(false, BR13_COLOR_DEPTH_32BIT, BR13_RASTER_OPERATION_PATTERN,
             dst_pitch >> 2),
        br22(dst_y1, dst_x1),
        br23(dst_y2, dst_x2),
        br09(dst->offset),
        br16(color)
    );
}

#endif

