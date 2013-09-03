#ifndef SWC_TTY_H
#define SWC_TTY_H 1

#include <stdint.h>
#include <stdbool.h>
#include <signal.h>

enum swc_tty_event
{
    SWC_TTY_VT_ENTER = 0,
    SWC_TTY_VT_LEAVE
};

struct swc_tty
{
    int fd;
    uint8_t vt;

    bool active;

    /* The state of the VT the compositor was started on so we have a state to
     * restore to when the compositor closes. */
    struct
    {
        uint8_t vt;
        long kb_mode;
    } original_state;

    /* Receives events when switching from/to the VT the compositor is running on. */
    struct wl_event_source * vt_source;

    struct wl_signal event_signal;
};

bool swc_tty_initialize(struct swc_tty * tty,
                        struct wl_event_loop * event_loop,
                        uint8_t tty_number);

void swc_tty_finish(struct swc_tty * tty);

void swc_tty_switch_vt(struct swc_tty * tty, uint32_t vt);

#endif

