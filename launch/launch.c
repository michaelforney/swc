#include "swc-launch.h"
#include "protocol.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/major.h>
#include <linux/vt.h>
#include <xf86drm.h>

static void print_usage(const char * name, const char * compositor_name)
{
    fprintf(stderr, "Usage: %s [-- [%s arguments]]\n",
            name, compositor_name);
}

static void catch_chld(int signal)
{
    int status;

    wait(&status);
    exit(status);
}

static void handle_socket_data(int socket)
{
    char buffer[BUFSIZ];
    struct swc_launch_request * request = (void *) &buffer;
    struct swc_launch_response response = { false };
    int fd;
    ssize_t size;

    size = receive_fd(socket, &fd, buffer, sizeof buffer);

    if (size == -1 || size == 0)
        return;

    switch (request->type)
    {
        case SWC_LAUNCH_REQUEST_DRM_MASTER:
            response.success = (request->set ? drmSetMaster(fd)
                                             : drmDropMaster(fd)) == 0;
            fd = -1;
            break;
        case SWC_LAUNCH_REQUEST_OPEN_INPUT_DEVICE:
        {
            struct stat st;

            if (request->path[size - __builtin_offsetof(typeof(*request),
                                                        path)] != '\0')
            {
                fprintf(stderr, "Path is not NULL terminated\n");
                goto fail;
            }

            stat(request->path, &st);

            if (major(st.st_rdev) != INPUT_MAJOR)
            {
                fprintf(stderr, "Device is not an input device\n");
                goto fail;
            }

            fd = open(request->path, request->flags);

            if (fd == -1)
            {
                fprintf(stderr, "Could not open device %s\n", request->path);
                goto fail;
            }

            break;
        }
        default:
            fprintf(stderr, "Unknown request %u\n", request->type);
            goto fail;
    }

    goto done;

  fail:
    response.success = false;
    fd = -1;
  done:
    send_fd(socket, fd, &response, sizeof response);
}

static void monitor_socket(int epoll_fd)
{
    struct epoll_event event;
    int count;

    while (true)
    {
        printf("waiting\n");
        count = epoll_wait(epoll_fd, &event, 1, -1);

        if (count < 0)
            break;

        handle_socket_data(event.data.fd);
    }
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
        printf("vt: %d\n", vt);
        if (*end == '\0')
            goto done;
    }

    tty0_fd = open("/dev/tty0", O_RDWR);

    if (ioctl(tty0_fd, VT_OPENQRY, &vt) != 0)
    {
        printf("could not find open vt\n");
        vt = 0;
    }

    close(tty0_fd);

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
        {
            fprintf(stderr, "FATAL: Could not open %s\n", tty_name);
            exit(EXIT_FAILURE);
        }

        return fd;
    }
}

int swc_launch(int argc, const char * argv[], const char * path)
{
    int sockets[2];
    int epoll_fd;
    struct epoll_event event;

    if (socketpair(AF_LOCAL, SOCK_DGRAM, 0, sockets) == -1)
    {
        fprintf(stderr, "FATAL: Could not create socket pair\n");
        return EXIT_FAILURE;
    }

    if (fcntl(sockets[0], F_SETFD, FD_CLOEXEC) == -1)
    {
        fprintf(stderr, "FATAL: Could not set CLOEXEC on socket\n");
        return EXIT_FAILURE;
    }

    epoll_fd = epoll_create1(EPOLL_CLOEXEC);

    if (epoll_fd == -1)
    {
        fprintf(stderr, "FATAL: Could not create epoll\n");
        return EXIT_FAILURE;
    }

    event.events = EPOLLIN;
    event.data.fd = sockets[0];
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockets[0], &event);

    /* Child */
    if (fork() == 0)
    {
        char string[64];
        int tty_fd;
        int vt;
        uid_t uid;
        gid_t gid;

        vt = find_vt();
        tty_fd = open_tty(vt);

        /* If the desired TTY is not the current TTY, start a new session, and
         * set it's controlling TTY appropriately. */
        if (tty_fd != STDIN_FILENO)
        {
            pid_t sid = setsid();

            if (ioctl(tty_fd, TIOCSCTTY, vt) != 0)
            {
                fprintf(stderr, "FATAL: Couldn't set controlling TTY to "
                                "/dev/tty%u: %s\n", vt, strerror(errno));
                exit(EXIT_FAILURE);
            }

            //tcsetpgrp(tty_fd, sid);
        }

        sprintf(string, "%d", sockets[1]);
        setenv(SWC_LAUNCH_SOCKET_ENV, string, 1);

        sprintf(string, "%d", tty_fd);
        setenv(SWC_LAUNCH_TTY_FD_ENV, string, 1);

        printf("dropping privileges\n");
        setuid(getuid());
        setgid(getgid());
        //execlp("valgrind", "valgrind", "-v", "--suppressions=drm.supp",
        //       "--track-origins=yes", path, NULL);
        execlp("gdb", "gdb", path, NULL);
        //execl(path, path, NULL);

        printf("%s failed: %s\n", path, strerror(errno));

        exit(EXIT_FAILURE);
    }
    /* Parent */
    else
    {
        struct sigaction action;
        sigset_t blocked_signals;

        action.sa_handler = &catch_chld;
        sigaction(SIGCHLD, &action, NULL);

        sigemptyset(&blocked_signals);
        sigaddset(&blocked_signals, SIGINT);
        sigaddset(&blocked_signals, SIGTERM);
        sigprocmask(SIG_BLOCK, &blocked_signals, NULL);

        printf("monitoring socket\n");
        monitor_socket(epoll_fd);
    }

    return EXIT_SUCCESS;
}

