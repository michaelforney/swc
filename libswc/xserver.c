/* swc: libswc/xserver.c
 *
 * Copyright (c) 2013 Michael Forney
 *
 * Based in part upon xwayland/launcher.c from weston, which is
 *
 *     Copyright © 2011 Intel Corporation
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

#include "xserver.h"
#include "internal.h"
#include "util.h"
#include "xwm.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <wayland-server.h>
#include "protocol/xserver-server-protocol.h"

#define LOCK_FMT    "/tmp/.X%d-lock"
#define SOCKET_DIR  "/tmp/.X11-unix"
#define SOCKET_FMT  SOCKET_DIR "/X%d"

static struct
{
    struct wl_global * global;
    struct wl_client * client;
    struct wl_resource * resource;
    int display;
    char display_name[16];
    int abstract_socket, unix_socket;
} xserver;

static char * xserver_command[] = {
    "X", "-wayland", "-rootless", "-nolisten", "all", xserver.display_name, NULL
};

static void set_window_id(struct wl_client * client,
                          struct wl_resource * resource,
                          struct wl_resource * surface_resource, uint32_t id)
{
    struct swc_surface * surface = wl_resource_get_user_data(surface_resource);

    swc_xwm_manage_window(id, surface);
}

const static struct xserver_interface xserver_implementation = {
    .set_window_id = &set_window_id
};

static int open_socket(struct sockaddr_un * addr, size_t path_size)
{
    int fd;
    socklen_t size = OFFSET_OF(typeof(*addr), sun_path) + path_size + 1;

    if ((fd = socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0)) < 0)
        goto error0;

    if (bind(fd, (struct sockaddr *) addr, size) < 0)
        goto error1;

    if (listen(fd, 1) < 0)
        goto error2;

    return fd;

  error2:
    if (addr->sun_path[0])
        unlink(addr->sun_path);
  error1:
    close(fd);
  error0:
    return -1;
}

static bool open_display()
{
    char lock_name[64], pid[12];
    int lock_fd;
    struct sockaddr_un addr = { .sun_family = AF_LOCAL };
    size_t path_size;

    xserver.display = 0;

    /* Create X lockfile and server sockets */
    goto begin;

  retry2:
    close(xserver.abstract_socket);
  retry1:
    unlink(lock_name);
  retry0:
    if (++xserver.display > 32)
    {
        ERROR("No open display in first 32\n");
        return false;
    }

  begin:
    snprintf(lock_name, sizeof lock_name, LOCK_FMT, xserver.display);
    lock_fd = open(lock_name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0444);

    /* XXX: Stale lockfile handling? */
    if (lock_fd < 0)
        goto retry0;

    snprintf(pid, sizeof pid, "%10d\n", getpid());
    if (write(lock_fd, pid, sizeof pid) != sizeof pid)
    {
        ERROR("Failed to write PID file\n");
        unlink(lock_name);
        close(lock_fd);
        return false;
    }

    close(lock_fd);

    /* Bind to abstract socket */
    addr.sun_path[0] = '\0';
    path_size = snprintf(addr.sun_path + 1, sizeof addr.sun_path - 1,
                         SOCKET_FMT, xserver.display);
    if ((xserver.abstract_socket = open_socket(&addr, path_size)) < 0)
        goto retry1;

    /* Bind to unix socket */
    mkdir(SOCKET_DIR, 0777);
    path_size = snprintf(addr.sun_path, sizeof addr.sun_path,
                         SOCKET_FMT, xserver.display);
    if ((xserver.unix_socket = open_socket(&addr, path_size)) < 0)
        goto retry2;

    snprintf(xserver.display_name, sizeof xserver.display_name,
             ":%d", xserver.display);
    setenv("DISPLAY", xserver.display_name, true);

    return true;
}

static void close_display()
{
    char path[64];

    close(xserver.abstract_socket);
    close(xserver.unix_socket);

    snprintf(path, sizeof path, SOCKET_FMT, xserver.display);
    unlink(path);
    snprintf(path, sizeof path, LOCK_FMT, xserver.display);
    unlink(path);

    unsetenv("DISPLAY");
}

static void bind_xserver(struct wl_client * client, void * data,
                         uint32_t version, uint32_t id)
{
    int sv[2];

    if (client != xserver.client)
        return;

    if (version >= 1)
        version = 1;

    DEBUG("Binding X server\n");

    xserver.resource = wl_resource_create(client, &xserver_interface,
                                          version, id);
    wl_resource_set_implementation(xserver.resource, &xserver_implementation,
                                   NULL, NULL);

    /* Start the X window manager */
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    xserver_send_client(xserver.resource, sv[1]);
    close(sv[1]);

    /* Need to flush the xserver client so the X window manager can connect to
     * it's socket. */
    wl_client_flush(xserver.client);
    swc_xwm_initialize(sv[0]);

    xserver_send_listen_socket(xserver.resource, xserver.abstract_socket);
    xserver_send_listen_socket(xserver.resource, xserver.unix_socket);
}

static bool start_xserver()
{
    int sv[2];

    /* Open an X display */
    if (!open_display())
    {
        ERROR("Failed to get X lockfile and sockets\n");
        goto error0;
    }

    /* Start the X server */
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) == -1)
    {
        ERROR("Failed to create socketpair: %s\n", strerror(errno));
        goto error1;
    }

    if (!(xserver.client = wl_client_create(swc.display, sv[0])))
        goto error2;

    switch (fork())
    {
        case 0:
        {
            int socket_fd;
            char socket_string[32];

            if (!(socket_fd = dup(sv[1])))
                exit(EXIT_FAILURE);

            snprintf(socket_string, sizeof socket_string, "%d", socket_fd);
            setenv("WAYLAND_SOCKET", socket_string, true);

            execvp(xserver_command[0], xserver_command);
            exit(EXIT_FAILURE);

            break;
        }
        case -1:
            ERROR("fork() failed when trying to start X server: %s\n",
                  strerror(errno));
            goto error2;
    }

    close(sv[1]);

    return true;

  error2:
    close(sv[1]);
    close(sv[0]);
  error1:
    close_display();
  error0:
    return false;
}

bool swc_xserver_initialize()
{
    xserver.global = wl_global_create(swc.display, &xserver_interface, 1,
                                      NULL, &bind_xserver);

    if (!xserver.global)
        goto error0;

    if (!start_xserver())
        goto error1;

    return true;

  error1:
    wl_global_destroy(xserver.global);
  error0:
    return false;
}

void swc_xserver_finalize()
{
    swc_xwm_finalize();
    close_display();
    wl_global_destroy(xserver.global);
}

