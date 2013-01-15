#ifndef SWC_I915_BLT_H
#define SWC_I915_BLT_H 1

#include "i915/bo.h"
#include "i915/batch.h"

#define BR00_CLIENT_2D 0x2

#define BR00_OPCODE_XY_COLOR_BLT 0x50
#define BR00_OPCODE_XY_SRC_COPY_BLT 0x53

#define BR00_32BPP_BYTE_MASK_ALPHA (1 << 1)
#define BR00_32BPP_BYTE_MASK_COLOR (1 << 2)

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

static inline void xy_src_copy_blt(struct i915_batch * batch,
                                   struct i915_bo * src, uint16_t src_pitch,
                                   uint16_t src_x, uint16_t src_y,
                                   struct i915_bo * dst, uint16_t dst_pitch,
                                   uint16_t dst_x, uint16_t dst_y,
                                   uint16_t width, uint16_t height)
{
#if 0
    i915_batch_add_dword(batch, 
    uint32_t * commands = i915_batch_alloc(batch, 8);
    commands = (uint32_t *)
    *commands++ = br00(BR00_CLIENT_2D, BR00_OPCODE_XY_SRC_COPY_BLT,
                       BR00_32BPP_BYTE_MASK_ALPHA | BR00_32BPP_BYTE_MASK_COLOR,
                       false, false, 6);
    *commands++ = br13(false, BR13_COLOR_DEPTH_32BIT,
                       BR13_RASTER_OPERATION_SRC,
                       dst_pitch);
    *commands++ = br22(dst_y, dst_x);
    *commands++ = br23(dst_y + height, dst_x + width);
    *commands++ = br09(0); /* XXX: dst address */
    *commands++ = br26(src_y, src_x);
    *commands++ = br11(src_pitch);
    *commands++ = br12(0); /* XXX: src address */
#endif

    uint32_t dst_address, src_address;

    i915_batch_ensure_space(batch, 8);

    dst_address = i915_batch_add_relocation(batch, 4, dst,
                                            I915_GEM_DOMAIN_RENDER,
                                            I915_GEM_DOMAIN_RENDER);
    src_address = i915_batch_add_relocation(batch, 7, src,
                                            I915_GEM_DOMAIN_RENDER, 0);

    i915_batch_add_dwords(batch, 8,
        br00(BR00_CLIENT_2D, BR00_OPCODE_XY_SRC_COPY_BLT,
             BR00_32BPP_BYTE_MASK_ALPHA | BR00_32BPP_BYTE_MASK_COLOR,
             false, false, 6),
        br13(false, BR13_COLOR_DEPTH_32BIT, BR13_RASTER_OPERATION_SOURCE,
             dst_pitch),
        br22(dst_y, dst_x),
        br23(dst_y + height, dst_x + width),
        br09(dst_address),
        br26(src_y, src_x),
        br11(src_pitch),
        br12(src_address)
    );
}

static inline void xy_color_blt(struct i915_batch * batch,
                                struct i915_bo * dst, uint16_t dst_pitch,
                                uint16_t dst_x, uint16_t dst_y,
                                uint16_t width, uint16_t height,
                                uint32_t color)
{
    uint32_t dst_address;

    i915_batch_ensure_space(batch, 6);

    dst_address = i915_batch_add_relocation(batch, 4, dst,
                                            I915_GEM_DOMAIN_RENDER,
                                            I915_GEM_DOMAIN_RENDER);

    i915_batch_add_dwords(batch, 6,
        br00(BR00_CLIENT_2D, BR00_OPCODE_XY_COLOR_BLT,
             BR00_32BPP_BYTE_MASK_ALPHA | BR00_32BPP_BYTE_MASK_COLOR,
             false, false, 4),
        br13(false, BR13_COLOR_DEPTH_32BIT, BR13_RASTER_OPERATION_PATTERN,
             dst_pitch),
        br22(dst_y, dst_x),
        br23(dst_y + height, dst_x + width),
        br09(dst_address),
        br16(color)
    );
}

#endif

