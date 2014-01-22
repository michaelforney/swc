#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

#ifndef SWC_XKB_H
#define SWC_XKB_H

/* Keycodes are offset by 8 in XKB. */
#define XKB_KEY(key) ((key) + 8)

struct swc_xkb
{
    struct xkb_context * context;
    struct xkb_state * state;

    struct
    {
        struct xkb_keymap * map;
        int fd;
        uint32_t size;
        char * area;
    } keymap;

    struct
    {
        uint32_t ctrl, alt, super, shift;
    } indices;
};

bool swc_xkb_initialize(struct swc_xkb * xkb);
void swc_xkb_finalize(struct swc_xkb * xkb);

bool swc_xkb_update_keymap(struct swc_xkb * xkb);

#endif

