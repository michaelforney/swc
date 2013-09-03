#include "util.h"
#include "launch/protocol.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <drm.h>
#include <sys/socket.h>

void swc_remove_resource(struct wl_resource * resource)
{
    wl_list_remove(wl_resource_get_link(resource));
}

bool swc_launch_drm_master(int socket, int fd, bool set)
{
    ssize_t size;
    struct swc_launch_request request;
    struct swc_launch_response response;

    request.type = SWC_LAUNCH_REQUEST_DRM_MASTER;
    request.set = set;

    size = send_fd(socket, fd, &request, sizeof request);

    if (size == -1)
        return false;

    size = recv(socket, &response, sizeof response, 0);

    if (size == -1)
        return false;

    return true;
}

int swc_launch_open_input_device(int socket, const char * path, int flags)
{
    size_t path_size = strlen(path);
    char buffer[sizeof(struct swc_launch_request) + path_size + 1];
    struct swc_launch_request * request = (void *) buffer;
    struct swc_launch_response response;
    ssize_t size;
    int fd;

    request->type = SWC_LAUNCH_REQUEST_OPEN_INPUT_DEVICE;
    request->flags = flags;
    strcpy(request->path, path);

    size = send(socket, buffer, sizeof buffer, 0);

    if (size == -1)
        return -1;

    size = receive_fd(socket, &fd, &response, sizeof response);

    if (size == -1)
        return -1;

    return fd;
}

