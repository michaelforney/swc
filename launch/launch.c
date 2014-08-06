/* swc: launch/launch.c
 *
 * Copyright (c) 2013, 2014 Michael Forney
 *
 * Based in part upon weston-launch.c from weston which is:
 *
 *     Copyright © 2012 Benjamin Franzke
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

#include "protocol.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <linux/major.h>
#include <linux/vt.h>
#include <xf86drm.h>

#ifndef DRM_MAJOR
# define DRM_MAJOR 226
#endif
#ifndef EVIOCREVOKE
# define EVIOCREVOKE _IOW('E', 0x91, int)
#endif

#define ARRAY_LENGTH(array) (sizeof (array) / sizeof (array)[0])

pid_t child_pid = -1;

static struct
{
    int socket;
    int input_fds[128];
    unsigned num_input_fds;
    int drm_fds[16];
    unsigned num_drm_fds;
    int tty_fd;
} launcher;

static struct
{
    bool altered;
    int vt;
    long kb_mode;
    long console_mode;
} original_vt_state;

static void __attribute__((noreturn,format(printf,1,2)))
    die(const char * format, ...);

static void __attribute__((noreturn)) usage(const char * name)
{
    fprintf(stderr, "Usage: %s [-s <server-socket>] [--] "
                    "<server> [server arguments...]\n", name);
    exit(EXIT_FAILURE);
}

static void start_devices()
{
    unsigned index;

    for (index = 0; index < launcher.num_drm_fds; ++index)
    {
        if (drmSetMaster(launcher.drm_fds[index]) < 0)
            die("Failed to set DRM master");
    }
}

static void stop_devices(bool fatal)
{
    unsigned index;

    for (index = 0; index < launcher.num_drm_fds; ++index)
    {
        if (drmDropMaster(launcher.drm_fds[index]) < 0 && fatal)
            die("Failed to drop DRM master");
    }

    for (index = 0; index < launcher.num_input_fds; ++index)
    {
        if (ioctl(launcher.input_fds[index], EVIOCREVOKE, 0) == -1
            && errno != ENODEV && fatal)
        {
            die("FATAL: Your kernel does not support EVIOCREVOKE; "
                "input devices cannot be revoked: %s", strerror(errno));
        }

        close(launcher.input_fds[index]);
    }

    launcher.num_input_fds = 0;
}

static void cleanup()
{
    /* Cleanup VT */
    if (original_vt_state.altered)
    {
        struct vt_mode mode = { .mode = VT_AUTO };

        fprintf(stderr, "Restoring VT to original state\n");
        ioctl(launcher.tty_fd, VT_SETMODE, &mode);
        ioctl(launcher.tty_fd, KDSETMODE, original_vt_state.console_mode);
        ioctl(launcher.tty_fd, KDSKBMODE, original_vt_state.kb_mode);
        ioctl(launcher.tty_fd, VT_ACTIVATE, original_vt_state.vt);
    }

    stop_devices(false);
}

void __attribute__((noreturn,format(printf,1,2)))
    die(const char * format, ...)
{
    va_list args;

    fputs("FATAL: ", stderr);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    if (errno != 0)
        fprintf(stderr, ": %s", strerror(errno));

    fputc('\n', stderr);

    if (child_pid)
        cleanup();

    exit(EXIT_FAILURE);
}

static void handle_chld(int signal)
{
    int status;

    wait(&status);
    fprintf(stderr, "Server exited with status %d\n", WEXITSTATUS(status));
    cleanup();
    exit(WEXITSTATUS(status));
}

static void handle_usr1(int signal)
{
    struct swc_launch_event event = { .type = SWC_LAUNCH_EVENT_DEACTIVATE };

    send(launcher.socket, &event, sizeof event, 0);
    stop_devices(true);
    ioctl(launcher.tty_fd, VT_RELDISP, 1);
}

static void handle_usr2(int signal)
{
    struct swc_launch_event event = { .type = SWC_LAUNCH_EVENT_ACTIVATE };

    ioctl(launcher.tty_fd, VT_RELDISP, VT_ACKACQ);
    start_devices();
    send(launcher.socket, &event, sizeof event, 0);
}

static void forward_signal(int signal)
{
    kill(child_pid, signal);
}

static void handle_socket_data(int socket)
{
    char buffer[BUFSIZ];
    struct swc_launch_request * request = (void *) &buffer;
    struct swc_launch_event response;
    int fd = -1;
    struct stat st;
    ssize_t size;

    size = receive_fd(socket, &fd, buffer, sizeof buffer);

    if (size == -1 || size == 0)
        return;

    response.type = SWC_LAUNCH_EVENT_RESPONSE;
    response.serial = request->serial;

    switch (request->type)
    {
        case SWC_LAUNCH_REQUEST_OPEN_DEVICE:
            if (request->path[size - __builtin_offsetof(typeof(*request),
                                                        path) - 1] != '\0')
            {
                fprintf(stderr, "Path is not NULL terminated\n");
                goto fail;
            }

            if (stat(request->path, &st) == -1)
            {
                fprintf(stderr, "Could not stat %s\n", request->path);
                goto fail;
            }

            switch (major(st.st_rdev))
            {
                case INPUT_MAJOR:
                    if (launcher.num_input_fds
                        == ARRAY_LENGTH(launcher.input_fds))
                    {
                        fprintf(stderr, "Too many input devices opened\n");
                        goto fail;
                    }
                    break;
                case DRM_MAJOR:
                    if (launcher.num_drm_fds
                        == ARRAY_LENGTH(launcher.drm_fds))
                    {
                        fprintf(stderr, "Too many DRM devices opened\n");
                        goto fail;
                    }
                    break;
                default:
                    fprintf(stderr, "Device is not an input device\n");
                    goto fail;
            }

            fd = open(request->path, request->flags);

            if (fd == -1)
            {
                fprintf(stderr, "Could not open device %s\n", request->path);
                goto fail;
            }

            switch (major(st.st_rdev))
            {
                case INPUT_MAJOR:
                    launcher.input_fds[launcher.num_input_fds++] = fd;
                    break;
                case DRM_MAJOR:
                    if (drmSetMaster(fd) == -1)
                    {
                        perror("Could not become DRM master");
                        goto fail;
                    }
                    launcher.drm_fds[launcher.num_drm_fds++] = fd;
                    break;
            }

            break;
        case SWC_LAUNCH_REQUEST_ACTIVATE_VT:
            if (ioctl(launcher.tty_fd, VT_ACTIVATE, request->vt) == -1)
            {
                fprintf(stderr, "Could not activate VT %d: %s\n",
                        request->vt, strerror(errno));
            }
            break;
        default:
            fprintf(stderr, "Unknown request %u\n", request->type);
            goto fail;
    }

    response.success = true;
    goto done;

  fail:
    response.success = false;
    fd = -1;
  done:
    send_fd(socket, fd, &response, sizeof response);
}

static int find_vt()
{
    char * vt_string;
    int vt;
    int tty0_fd;

    vt_string = getenv("XDG_VTNR");

    if (vt_string)
    {
        char * end;
        vt = strtoul(vt_string, &end, 10);
        if (*end == '\0')
            goto done;
    }

    tty0_fd = open("/dev/tty0", O_RDWR);

    if (tty0_fd == -1)
        die("Could not open /dev/tty0 to find unused VT");

    if (ioctl(tty0_fd, VT_OPENQRY, &vt) != 0)
        die("Could not find unused VT");

    close(tty0_fd);

    fprintf(stderr, "Running on VT %d\n", vt);

  done:
    return vt;
}

static int open_tty(int vt)
{
    char * current_tty_name;
    char tty_name[64];

    snprintf(tty_name, sizeof tty_name, "/dev/tty%d", vt);

    /* Check if we are running on the desired VT */
    current_tty_name = ttyname(STDIN_FILENO);

    if (strcmp(tty_name, current_tty_name) == 0)
        return STDIN_FILENO;
    else
    {
        int fd;

        /* Open the new TTY. */
        fd = open(tty_name, O_RDWR | O_NOCTTY);

        if (fd < 0)
            die("Could not open %s", tty_name);

        return fd;
    }
}

static void setup_tty(int fd)
{
    struct stat st;
    int vt;
    struct vt_stat state;
    struct vt_mode mode = {
        .mode = VT_PROCESS,
        .relsig = SIGUSR1,
        .acqsig = SIGUSR2
    };

    if (fstat(fd, &st) == -1)
        die("Could not stat TTY fd");

    vt = minor(st.st_rdev);

    if (major(st.st_rdev) != TTY_MAJOR || vt == 0)
        die("Not a valid VT");

    if (ioctl(fd, VT_GETSTATE, &state) == -1)
        die("Could not get the current VT state");

    original_vt_state.vt = state.v_active;

    if (ioctl(fd, KDGKBMODE, &original_vt_state.kb_mode))
        die("Could not get keyboard mode");

    if (ioctl(fd, KDGETMODE, &original_vt_state.console_mode))
        die("Could not get console mode");

    if (ioctl(fd, KDSKBMODE, K_OFF) == -1)
        die("Could not set keyboard mode to K_OFF");

    if (ioctl(fd, KDSETMODE, KD_GRAPHICS) == -1)
    {
        perror("Could not set console mode to KD_GRAPHICS");
        goto error0;
    }

    if (ioctl(fd, VT_SETMODE, &mode) == -1)
    {
        perror("Could not set VT mode");
        goto error1;
    }

    if (ioctl(fd, VT_ACTIVATE, vt) == -1)
    {
        perror("Could not activate VT");
        goto error2;
    }

    if (ioctl(fd, VT_WAITACTIVE, vt) == -1)
    {
        perror("Could not wait for VT to become active");
        goto error2;
    }

    original_vt_state.altered = true;

    return;

  error2:
    mode = (struct vt_mode) { .mode = VT_AUTO };
    ioctl(fd, VT_SETMODE, &mode);
  error1:
    ioctl(fd, KDSETMODE, original_vt_state.console_mode);
  error0:
    ioctl(fd, KDSKBMODE, original_vt_state.kb_mode);
    exit(EXIT_FAILURE);
}

int main(int argc, char * argv[])
{
    int option;
    int sockets[2];
    int vt;
    struct sigaction action = { 0 };
    sigset_t set;

    while ((option = getopt(argc, argv, "hs:")) != -1)
    {
        switch (option)
        {
            case 's':
                setenv("WAYLAND_DISPLAY", optarg, true);
                break;
            case 'h':
            default:
                usage(argv[0]);
        }
    }

    if (argc - optind < 1)
        usage(argv[0]);

    if (socketpair(AF_LOCAL, SOCK_SEQPACKET, 0, sockets) == -1)
        die("Could not create socket pair");

    launcher.socket = sockets[0];

    if (fcntl(sockets[0], F_SETFD, FD_CLOEXEC) == -1)
        die("Could not set CLOEXEC on socket");

    action.sa_handler = &handle_chld;
    if (sigaction(SIGCHLD, &action, NULL) == -1)
        die("Failed to register signal handler for SIGCHLD");

    action.sa_handler = &handle_usr1;
    if (sigaction(SIGUSR1, &action, NULL) == -1)
        die("Failed to register signal handler for SIGUSR1");

    action.sa_handler = &handle_usr2;
    if (sigaction(SIGUSR2, &action, NULL) == -1)
        die("Failed to register signal handler for SIGUSR2");

    action.sa_handler = &forward_signal;
    if (sigaction(SIGINT, &action, NULL) == -1)
        die("Failed to register signal handler for SIGINT");
    if (sigaction(SIGTERM, &action, NULL) == -1)
        die("Failed to register signal handler for SIGTERM");

    sigfillset(&set);
    sigdelset(&set, SIGCHLD);
    sigdelset(&set, SIGUSR1);
    sigdelset(&set, SIGUSR2);
    sigdelset(&set, SIGINT);
    sigdelset(&set, SIGTERM);
    sigprocmask(SIG_SETMASK, &set, NULL);

    vt = find_vt();
    launcher.tty_fd = open_tty(vt);
    setup_tty(launcher.tty_fd);

    child_pid = fork();

    /* Child */
    if (child_pid == 0)
    {
        char string[64];

        /* Reset signal handlers to defaults */
        action.sa_handler = SIG_DFL;
        if (sigaction(SIGCHLD, &action, NULL) == -1)
            die("Failed to set default signal handler for SIGCHLD");
        if (sigaction(SIGUSR1, &action, NULL) == -1)
            die("Failed to set default signal handler for SIGUSR1");
        if (sigaction(SIGUSR2, &action, NULL) == -1)
            die("Failed to set default signal handler for SIGUSR2");
        if (sigaction(SIGINT, &action, NULL) == -1)
            die("Failed to set default signal handler for SIGINT");
        if (sigaction(SIGTERM, &action, NULL) == -1)
            die("Failed to set default signal handler for SIGTERM");

        /* Set empty signal mask */
        sigemptyset(&set);
        sigprocmask(SIG_SETMASK, &set, NULL);

        sprintf(string, "%d", sockets[1]);
        setenv(SWC_LAUNCH_SOCKET_ENV, string, 1);

        sprintf(string, "%d", launcher.tty_fd);
        setenv(SWC_LAUNCH_TTY_FD_ENV, string, 1);

        if (setuid(getuid()) != 0 || setgid(getgid()) != 0)
            die("Failed to drop permission before executing display server");

        execvp(argv[optind], argv + optind);
        die("Could not exec %s", argv[optind]);
    }
    /* Parent */
    else
    {
        struct pollfd pollfd;
        int ret;

        pollfd.fd = sockets[0];
        pollfd.events = POLLIN;

        while (true)
        {
            ret = poll(&pollfd, 1, -1);

            if (ret == -1)
            {
                if (errno == EINTR)
                    continue;
                else
                    die("Error while polling on socket fd");
            }

            handle_socket_data(pollfd.fd);
        }
    }

    return EXIT_SUCCESS;
}

