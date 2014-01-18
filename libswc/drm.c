/* swc: drm.c
 *
 * Copyright (c) 2013 Michael Forney
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "drm.h"
#include "event.h"
#include "internal.h"
#include "output.h"
#include "screen.h"
#include "util.h"
#include "wayland_buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <libudev.h>
#include <libdrm/drm.h>
#include <xf86drm.h>
#include <wld/wld.h>
#include <wld/drm.h>
#include <wayland-server.h>
#include "protocol/wayland-drm-server-protocol.h"

struct swc_drm drm_global;

static struct
{
    uint32_t id;
    char * path;

    uint32_t taken_ids;

    struct wl_global * global;
    struct wl_event_source * event_source;
} drm;

static void authenticate(struct wl_client * client,
                         struct wl_resource * resource, uint32_t magic)
{
    int ret;

    if ((ret = drmAuthMagic(swc.drm->fd, magic)) == 0)
        wl_drm_send_authenticated(resource);
    else
    {
        wl_resource_post_error(resource, WL_DRM_ERROR_AUTHENTICATE_FAIL,
                               "drmAuthMagic failed: %d\n", ret);
    }
}

static void create_buffer(struct wl_client * client,
                          struct wl_resource * resource, uint32_t id,
                          uint32_t name, int32_t width, int32_t height,
                          uint32_t stride, uint32_t format)
{
    struct wld_buffer * wld;
    struct swc_buffer * buffer;
    union wld_object object = { .u32 = name };

    wld = wld_import_buffer(swc.drm->context, WLD_DRM_OBJECT_GEM_NAME, object,
                            width, height, format, stride);

    if (!wld)
        goto error0;

    buffer = swc_wayland_buffer_new(client, id, wld);

    if (!buffer)
        goto error1;

    return;

  error1:
    wld_destroy_buffer(wld);
  error0:
    wl_resource_post_no_memory(resource);
}

static void create_planar_buffer(struct wl_client * client,
                                 struct wl_resource * resource, uint32_t id,
                                 uint32_t name, int32_t width, int32_t height,
                                 uint32_t format,
                                 int32_t offset0, int32_t stride0,
                                 int32_t offset1, int32_t stride1,
                                 int32_t offset2, int32_t stride2)
{
    wl_resource_post_error(resource, WL_DRM_ERROR_INVALID_FORMAT,
                           "planar buffers are not supported\n");
}

static void create_prime_buffer(struct wl_client * client,
                                struct wl_resource * resource, uint32_t id,
                                int32_t fd, int32_t width, int32_t height,
                                uint32_t format,
                                int32_t offset0, int32_t stride0,
                                int32_t offset1, int32_t stride1,
                                int32_t offset2, int32_t stride2)
{
    struct wld_buffer * wld;
    struct swc_buffer * buffer;
    union wld_object object = { .i = fd };

    wld = wld_import_buffer(swc.drm->context, WLD_DRM_OBJECT_PRIME_FD, object,
                            width, height, format, stride0);
    close(fd);

    if (!wld)
        goto error0;

    buffer = swc_wayland_buffer_new(client, id, wld);

    if (!buffer)
        goto error1;

    return;

  error1:
    wld_destroy_buffer(wld);
  error0:
    wl_resource_post_no_memory(resource);
}

static const struct wl_drm_interface drm_implementation = {
        .authenticate = &authenticate,
        .create_buffer = &create_buffer,
        .create_planar_buffer = &create_planar_buffer,
        .create_prime_buffer = &create_prime_buffer
};

static struct udev_device * find_primary_drm_device(const char * seat)
{
    struct udev_enumerate * enumerate;
    struct udev_list_entry * entry;
    const char * path;
    const char * device_seat;
    const char * boot_vga;
    struct udev_device * pci;
    struct udev_device * device, * drm_device = NULL;

    enumerate = udev_enumerate_new(swc.udev);
    udev_enumerate_add_match_subsystem(enumerate, "drm");
    udev_enumerate_add_match_sysname(enumerate, "card[0-9]*");

    udev_enumerate_scan_devices(enumerate);

    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate))
    {
        path = udev_list_entry_get_name(entry);
        device = udev_device_new_from_syspath(swc.udev, path);

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

static bool find_available_crtc(drmModeRes * resources,
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
        encoder = drmModeGetEncoder(swc.drm->fd,
                                    connector->encoders[encoder_index]);
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

static bool find_available_id(uint32_t * id)
{
    uint32_t index = __builtin_ffsl(~drm.taken_ids);

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
    struct swc_drm_handler * handler = data;

    handler->page_flip(handler, sec * 1000 + usec / 1000);
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

static void bind_drm(struct wl_client * client, void * data, uint32_t version,
                     uint32_t id)
{
    struct wl_resource * resource;

    if (version >= 2)
        version = 2;

    resource = wl_resource_create(client, &wl_drm_interface, version, id);
    wl_resource_set_implementation(resource, &drm_implementation, NULL, NULL);

    if (version >= 2)
        wl_drm_send_capabilities(resource, WL_DRM_CAPABILITY_PRIME);

    wl_drm_send_device(resource, drm.path);
    wl_drm_send_format(resource, WL_DRM_FORMAT_XRGB8888);
    wl_drm_send_format(resource, WL_DRM_FORMAT_ARGB8888);
}

bool swc_drm_initialize(const char * seat_name)
{
    const char * sysnum;
    char * end;
    struct udev_device * drm_device;

    if (!(drm_device = find_primary_drm_device(seat_name)))
    {
        printf("couldn't find drm device\n");
        goto error0;
    }

    /* XXX: Why do we need the sysnum? */
    sysnum = udev_device_get_sysnum(drm_device);
    drm.id = strtoul(sysnum, &end, 10);

    if (*end != '\0')
    {
        printf("couldn't get drm device sysnum\n");
        udev_device_unref(drm_device);
        goto error0;
    }

    drm.taken_ids = 0;
    drm.path = strdup(udev_device_get_devnode(drm_device));
    udev_device_unref(drm_device);
    swc.drm->fd = swc_launch_open_device(drm.path, O_RDWR | O_CLOEXEC);

    if (swc.drm->fd == -1)
    {
        fprintf(stderr, "Could not open %s\n", drm.path);
        goto error1;
    }

    if (!(swc.drm->context = wld_drm_create_context(swc.drm->fd)))
    {
        fprintf(stderr, "Could not create WLD DRM context\n");
        goto error2;
    }

    if (!(swc.drm->renderer = wld_create_renderer(swc.drm->context)))
    {
        ERROR("Could not create WLD DRM renderer\n");
        goto error3;
    }

    drm.event_source = wl_event_loop_add_fd
        (swc.event_loop, swc.drm->fd, WL_EVENT_READABLE, &handle_data, NULL);

    if (!drm.event_source)
    {
        ERROR("Could not create DRM event source\n");
        goto error4;
    }

    if (!wld_drm_is_dumb(swc.drm->context))
    {
        drm.global = wl_global_create(swc.display, &wl_drm_interface, 2,
                                      NULL, &bind_drm);

        if (!drm.global)
        {
            ERROR("Could not create wl_drm global\n");
            goto error5;
        }
    }

    return true;

  error5:
    wl_event_source_remove(drm.event_source);
  error4:
    wld_destroy_renderer(swc.drm->renderer);
  error3:
    wld_destroy_context(swc.drm->context);
  error2:
    close(swc.drm->fd);
  error1:
    free(drm.path);
  error0:
    return false;
}

void swc_drm_finalize()
{
    if (drm.global)
        wl_global_destroy(drm.global);
    wl_event_source_remove(drm.event_source);
    wld_destroy_renderer(swc.drm->renderer);
    wld_destroy_context(swc.drm->context);
    free(drm.path);
    close(swc.drm->fd);
}

bool swc_drm_create_screens(struct wl_list * screens)
{
    drmModeRes * resources;
    drmModeConnector * connector;
    drmModeCrtc * crtc;
    uint32_t index;
    struct swc_output * output;
    uint32_t taken_crtcs = 0;

    if (!(resources = drmModeGetResources(swc.drm->fd)))
    {
        printf("couldn't get DRM resources\n");
        return false;
    }

    printf("crtc count: %u\n", resources->count_crtcs);

    /* XXX: crtcs */
    for (index = 0; index < resources->count_crtcs; ++index)
    {
        printf("crtc[%u]: %u\n", index, resources->crtcs[index]);
        crtc = drmModeGetCrtc(swc.drm->fd, resources->crtcs[index]);
        printf("crtc, id: %u, x: %u, y: %u, width: %u, height: %u\n",
            crtc->crtc_id, crtc->x, crtc->y, crtc->width, crtc->height);
        drmModeFreeCrtc(crtc);
    }

    for (index = 0; index < resources->count_encoders; ++index)
    {
        printf("encoder[%u]: %u\n", index, resources->encoders[index]);
        drmModeEncoder * encoder = drmModeGetEncoder
            (swc.drm->fd, resources->encoders[index]);
        printf("encoder, id: %u, type: %u\n", encoder->encoder_id, encoder->encoder_type);
        drmModeFreeEncoder(encoder);
    }

    for (index = 0; index < resources->count_connectors;
         ++index, drmModeFreeConnector(connector))
    {
        connector = drmModeGetConnector(swc.drm->fd,
                                        resources->connectors[index]);

        printf("connector, id: %u, type: %u, type_id: %u, connection: %u\n",
            connector->connector_id, connector->connector_type,
            connector->connector_type_id, connector->connection);

        /* XXX: connector id? */
        if (connector->connection == DRM_MODE_CONNECTED)
        {
            uint32_t crtc_index;
            uint32_t id;

            if (!find_available_crtc(resources, connector, taken_crtcs,
                                     &crtc_index))
            {
                printf("couldn't find crtc for connector %u\n", index);
                continue;
            }

            if (!find_available_id(&id))
            {
                printf("no more available output IDs\n");
                break;
            }

            if (!(output = swc_output_new(connector)))
                continue;

            output->screen = swc_screen_new(resources->crtcs[crtc_index],
                                            output);
            output->screen->id = id;

            taken_crtcs |= 1 << crtc_index;
            drm.taken_ids |= 1 << id;

            wl_list_insert(screens, &output->screen->link);
        }
    }

    drmModeFreeResources(resources);

    return true;
}

