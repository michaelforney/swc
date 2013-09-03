#ifndef SWC_EVENT_H
#define SWC_EVENT_H

#include <stdint.h>
#include <wayland-server.h>

struct swc_event
{
    uint32_t type;
    void * data;
};

static inline void swc_send_event(struct wl_signal * signal, uint32_t type,
                                  void * event_data)
{
    struct swc_event event = { .type = type, .data = event_data };

    wl_signal_emit(signal, &event);
}

#endif

