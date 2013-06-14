#ifndef SWC_DATA_DEVICE_H
#define SWC_DATA_DEVICE_H 1

#include <wayland-server.h>

struct swc_seat;

void swc_data_device_initialize(struct wl_client * client, uint32_t id,
                                struct swc_seat * seat);

#endif

