#include "data_device_manager.h"
#include "data_device.h"

#include <stdio.h>

static void create_data_source(struct wl_client * client,
                               struct wl_resource * resource, uint32_t id)
{
}

static void get_data_device(struct wl_client * client,
                            struct wl_resource * resource, uint32_t id,
                            struct wl_resource * seat_resource)
{
    struct swc_seat * seat = seat_resource->data;

    printf("get_data_device\n");

    // TODO: keep track of resource?
    swc_data_device_initialize(client, id, seat);
}

static struct wl_data_device_manager_interface
data_device_manager_implementation = {
    .create_data_source = &create_data_source,
    .get_data_device = &get_data_device
};

static void bind_data_device_manager(struct wl_client * client, void * data,
                                     uint32_t version, uint32_t id)
{
    wl_client_add_object(client, &wl_data_device_manager_interface,
                         &data_device_manager_implementation, id, NULL);
}

void swc_data_device_manager_add_globals(struct wl_display * display)
{
    wl_display_add_global(display, &wl_data_device_manager_interface, NULL,
                          &bind_data_device_manager);
}

