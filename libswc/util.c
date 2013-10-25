#include "util.h"
#include "launch/protocol.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <drm.h>
#include <sys/socket.h>
#include <wayland-server.h>

void swc_remove_resource(struct wl_resource * resource)
{
    wl_list_remove(wl_resource_get_link(resource));
}

static int get_launcher_socket()
{
    static int launcher_socket = -1;

    if (launcher_socket == -1)
    {
        char * launcher_socket_name;

        if ((launcher_socket_name = getenv(SWC_LAUNCH_SOCKET_ENV)))
        {
            char * end;

            launcher_socket = strtol(launcher_socket_name, &end, 10);
            if (*end != '\0')
                launcher_socket = -1;
        }
    }

    return launcher_socket;
}

static bool send_request(const struct swc_launch_request * request, size_t size,
                         struct swc_launch_response * response,
                         int out_fd, int * in_fd)
{
    int socket;
    ssize_t ret;

    socket = get_launcher_socket();

    if (send_fd(socket, out_fd, request, size) == -1)
        return false;

    if (receive_fd(socket, in_fd, &response, sizeof response) == -1)
        return false;

    return true;
}

int swc_launch_open_device(const char * path, int flags)
{
    size_t path_size = strlen(path);
    char buffer[sizeof(struct swc_launch_request) + path_size + 1];
    struct swc_launch_request * request = (void *) buffer;
    struct swc_launch_response response;
    int fd;
    int socket;

    socket = get_launcher_socket();

    request->type = SWC_LAUNCH_REQUEST_OPEN_DEVICE;
    request->flags = flags;
    strcpy(request->path, path);

    if (!send_request(request, sizeof buffer, &response, -1, &fd))
        return -1;

    return fd;
}

bool swc_launch_activate_vt(unsigned vt)
{
    struct swc_launch_request request;
    struct swc_launch_response response;
    ssize_t size;
    int socket;

    socket = get_launcher_socket();

    request.type = SWC_LAUNCH_REQUEST_ACTIVATE_VT;
    request.vt = vt;

    if (!send_request(&request, sizeof request, &response, -1, NULL))
        return false;

    return response.success;
}

