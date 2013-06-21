#include "data_device.h"

static void start_drag(struct wl_client * client, struct wl_resource * resource,
                       struct wl_resource * source_resource,
                       struct wl_resource * origin_resource,
                       struct wl_resource * icon_resource, uint32_t serial)
{
}

static void set_selection(struct wl_client * client,
                          struct wl_resource * resource,
                          struct wl_resource * source_resource, uint32_t serial)
{
}

struct wl_data_device_interface data_device_implementation = {
    .start_drag = &start_drag,
    .set_selection = &set_selection
};

void swc_data_device_new(struct wl_client * client, uint32_t id)
{
    wl_client_add_object(client, &wl_data_device_interface,
                         &data_device_implementation, id, NULL);
}

