#include "protocol.h"

#include <sys/socket.h>
#include <stdio.h>
#include <string.h>

ssize_t send_fd(int socket, int fd, const void * buffer, ssize_t buffer_size)
{
    char control[CMSG_SPACE(sizeof(int))];
    struct iovec iov = {
        .iov_base = (void *) buffer,
        .iov_len = buffer_size
    };
    struct msghdr message = {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = &iov,
        .msg_iovlen = 1,
    };
    struct cmsghdr * cmsg;

    if (fd != -1)
    {
        message.msg_control = control,
        message.msg_controllen = sizeof control;

        cmsg = CMSG_FIRSTHDR(&message);
        cmsg->cmsg_len = CMSG_LEN(sizeof fd);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;

        memcpy(CMSG_DATA(cmsg), &fd, sizeof fd);
    }
    else
    {
        message.msg_control = NULL;
        message.msg_controllen = 0;
    }

    return sendmsg(socket, &message, 0);
}

ssize_t receive_fd(int socket, int * fd, void * buffer,
                   ssize_t buffer_size)
{
    ssize_t size;

    if (fd)
    {
        char control[CMSG_SPACE(sizeof(int))];
        struct iovec iov = {
            .iov_base = buffer,
            .iov_len = buffer_size
        };
        struct msghdr message = {
            .msg_name = NULL,
            .msg_namelen = 0,
            .msg_iov = &iov,
            .msg_iovlen = 1,
            .msg_control = &control,
            .msg_controllen = sizeof control
        };
        struct cmsghdr * cmsg;

        size = recvmsg(socket, &message, 0);

        if (size < 0)
            goto nofd;

        cmsg = CMSG_FIRSTHDR(&message);

        if (!cmsg || cmsg->cmsg_len != CMSG_LEN(sizeof(int))
            || cmsg->cmsg_level != SOL_SOCKET
            || cmsg->cmsg_type != SCM_RIGHTS)
        {
            goto nofd;
        }

        memcpy(fd, CMSG_DATA(cmsg), sizeof *fd);
    }
    else
    {
        size = recv(socket, buffer, buffer_size, 0);
    }

    goto done;

  nofd:
    *fd = -1;
  done:
    return size;
}

