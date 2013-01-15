#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <libudev.h>
#include <libdrm/drm.h>
#include <xf86drm.h>
#include <libdrm/i915_drm.h>
//#include <xf86drmMode.h>
#include <wayland-util.h>

#include "drm.h"
#include "output.h"
#include "event.h"

static struct udev_device * find_primary_drm_device(struct udev * udev,
                                                    const char * seat)
{
    struct udev_enumerate * enumerate;
    struct udev_list_entry * entry;
    const char * path;
    const char * device_seat;
    const char * boot_vga;
    struct udev_device * pci;
    struct udev_device * device, * drm_device = NULL;

    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "drm");
    udev_enumerate_add_match_sysname(enumerate, "card[0-9]*");

    udev_enumerate_scan_devices(enumerate);

    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate))
    {
        path = udev_list_entry_get_name(entry);
        device = udev_device_new_from_syspath(udev, path);

        printf("device node path: %s\n", udev_device_get_devnode(device));

        device_seat = udev_device_get_property_value(device, "ID_SEAT");

        /* If the ID_SEAT property is not set, the device belongs to seat0. */
        if (!device_seat)
            device_seat = "seat0";
        else
            printf("device seat: %s\n", device_seat);

        /* Make sure the DRM device belongs to the seat we are in. */
        if (strcmp(device_seat, seat) != 0)
        {
            udev_device_unref(device);
            continue;
        }

        pci = udev_device_get_parent_with_subsystem_devtype(device, "pci",
                                                            NULL);

        if (pci)
        {
            /* boot_vga = 1 indicates that this DRM device is the primary GPU. */
            boot_vga = udev_device_get_sysattr_value(pci, "boot_vga");
            if (boot_vga && strcmp(boot_vga, "1") == 0)
            {
                if (drm_device)
                    udev_device_unref(drm_device);
                drm_device = device;
                break;
            }
        }

        /* Make sure we have a backup device. */
        if (!drm_device)
            drm_device = device;
        else
            udev_device_unref(device);
    }

    udev_enumerate_unref(enumerate);

    return drm_device;
}

static bool find_available_crtc(struct swc_drm * drm, drmModeRes * resources,
                                drmModeConnector * connector,
                                uint32_t taken_crtcs, uint32_t * crtc)
{
    uint32_t encoder_index, crtc_index;
    uint32_t possible_crtcs;
    drmModeEncoder * encoder;

    for (encoder_index = 0;
         encoder_index < connector->count_encoders;
         ++encoder_index)
    {
        encoder = drmModeGetEncoder(drm->fd, connector->encoders[encoder_index]);
        possible_crtcs = encoder->possible_crtcs;
        drmModeFreeEncoder(encoder);

        printf("possible_crtcs: %u\n", possible_crtcs);
        printf("taken_crtcs: %u\n", taken_crtcs);

        for (crtc_index = 0; crtc_index < resources->count_crtcs; ++crtc_index)
        {
            if ((possible_crtcs & (1 << crtc_index))
                && !(taken_crtcs & (1 << crtc_index)))
            {
                *crtc = crtc_index;
                return true;
            }
        }
    }

    return false;
}

static bool find_available_id(struct swc_drm * drm, uint32_t * id)
{
    uint32_t index = __builtin_ffsl(~drm->taken_output_ids);

    printf("drm->taken_output_ids: %u, index: %u\n", drm->taken_output_ids, index);

    if (index == 0)
        return false;

    *id = index - 1;
    return true;
}

static void handle_vblank(int fd, unsigned int sequence, unsigned int sec,
                          unsigned int usec, void * data)
{
    printf("vblank\n");
}

static void handle_page_flip(int fd, unsigned int sequence, unsigned int sec,
                             unsigned int usec, void * data)
{
    struct swc_output * output = data;
    struct swc_event event = {
        .type = SWC_DRM_PAGE_FLIP,
        .data = output
    };

    printf("page flip\n");
    output->front_buffer ^= 1;

    wl_signal_emit(&output->drm->event_signal, &event);
}

static drmEventContext event_context = {
    .version = DRM_EVENT_CONTEXT_VERSION,
    .vblank_handler = &handle_vblank,
    .page_flip_handler = &handle_page_flip
};

static int handle_data(int fd, uint32_t mask, void * data)
{
    drmHandleEvent(fd, &event_context);

    return 1;
}

bool swc_drm_initialize(struct swc_drm * drm, struct udev * udev,
                        const char * seat)
{
    const char * sysnum;
    const char * device_path;
    char * end;

    wl_signal_init(&drm->event_signal);

    struct udev_device * drm_device = find_primary_drm_device(udev, seat);

    if (!drm_device)
    {
        printf("couldn't find drm device\n");
        goto error_base;
    }

    /* XXX: Why do we need the sysnum? */
    sysnum = udev_device_get_sysnum(drm_device);
    drm->id = strtoul(sysnum, &end, 10);

    drm->taken_output_ids = 0;

    if (*end != '\0')
    {
        printf("couldn't get drm device sysnum\n");
        goto error_device;
    }

    printf("sysnum: %s\n", sysnum);

    device_path = udev_device_get_devnode(drm_device);
    drm->fd = open(device_path, O_RDWR | O_CLOEXEC);

    if (drm->fd == -1)
    {
        printf("couldn't open %s\n", device_path);
        goto error_device;
    }

    {
        int value, ret;
        struct drm_i915_getparam getparam = {
            .param = I915_PARAM_HAS_BLT,
            .value = &value
        };

        ret = drmIoctl(drm->fd, DRM_IOCTL_I915_GETPARAM, &getparam);

        printf("has blt: %u\n", ret);
    }

    udev_device_unref(drm_device);

    return true;

  error_device:
    udev_device_unref(drm_device);
  error_base:
    return false;
}

void swc_drm_finish(struct swc_drm * drm)
{
    close(drm->fd);
}

void swc_drm_add_event_sources(struct swc_drm * drm,
                               struct wl_event_loop * event_loop)
{
    drm->source = wl_event_loop_add_fd(event_loop, drm->fd, WL_EVENT_READABLE,
                                       &handle_data, NULL);
}

void swc_drm_set_master(struct swc_drm * drm)
{
    printf("setting drm master\n");
    drmSetMaster(drm->fd);
}

void swc_drm_drop_master(struct swc_drm * drm)
{
    printf("dropping drm master\n");
    drmDropMaster(drm->fd);
}

struct wl_list * swc_drm_create_outputs(struct swc_drm * drm)
{
    drmModeRes * resources;
    drmModeConnector * connector;
    drmModeCrtc * crtc;
    uint32_t index;
    uint32_t x = 0, y = 0;
    struct swc_output * output;
    struct wl_list * outputs;
    uint32_t taken_crtcs = 0;

    outputs = malloc(sizeof(struct wl_list));
    wl_list_init(outputs);

    resources = drmModeGetResources(drm->fd);
    if (!resources)
    {
        printf("couldn't get DRM resources\n");
        goto error;
    }

    printf("crtc count: %u\n", resources->count_crtcs);

    /* XXX: crtcs */
    for (index = 0; index < resources->count_crtcs; ++index)
    {
        printf("crtc[%u]: %u\n", index, resources->crtcs[index]);
        drmModeCrtc * crtc = drmModeGetCrtc(drm->fd, resources->crtcs[index]);
        printf("crtc, id: %u, x: %u, y: %u, width: %u, height: %u\n",
            crtc->crtc_id, crtc->x, crtc->y, crtc->width, crtc->height);
        drmModeFreeCrtc(crtc);
    }

    for (index = 0; index < resources->count_encoders; ++index)
    {
        printf("encoder[%u]: %u\n", index, resources->encoders[index]);
        drmModeEncoder * encoder = drmModeGetEncoder(drm->fd, resources->encoders[index]);
        printf("encoder, id: %u, type: %u\n", encoder->encoder_id, encoder->encoder_type);
        drmModeFreeEncoder(encoder);
    }

    for (index = 0; index < resources->count_connectors; ++index)
    {
        connector = drmModeGetConnector(drm->fd, resources->connectors[index]);

        printf("connector, id: %u, type: %u, type_id: %u, connection: %u\n",
            connector->connector_id, connector->connector_type,
            connector->connector_type_id, connector->connection);

        /* XXX: connector id? */
        if (connector->connection == DRM_MODE_CONNECTED)
        {
            uint32_t crtc_index;
            uint32_t id;

            if (!find_available_crtc(drm, resources, connector, taken_crtcs,
                                     &crtc_index))
            {
                printf("couldn't find crtc for connector %u\n", index);
                continue;
            }

            if (!find_available_id(drm, &id))
            {
                printf("no more available output IDs\n");
                break;
            }

            output = malloc(sizeof(struct swc_output));

            if (!swc_output_initialize(output, drm, id,
                                       resources->crtcs[crtc_index], connector))
            {
                drmModeFreeConnector(connector);
                free(output);
                continue;
            }

            output->x = x;
            output->y = 0;

            taken_crtcs |= 1 << crtc_index;
            drm->taken_output_ids |= 1 << id;

            wl_list_insert(outputs, &output->link);
            x += output->width;
        }

        drmModeFreeConnector(connector);
    }

    drmModeFreeResources(resources);

    if (wl_list_empty(outputs))
    {
        printf("couldn't find any outputs\n");
        goto error;
    }

    return outputs;

  error:
    free(outputs);
    return NULL;
}

