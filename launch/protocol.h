#ifndef SWC_LAUNCH_PROTOCOL_H
#define SWC_LAUNCH_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#define SWC_LAUNCH_SOCKET_ENV "SWC_LAUNCH_SOCKET"
#define SWC_LAUNCH_TTY_FD_ENV "SWC_LAUNCH_TTY_FD"

struct swc_launch_request
{
    enum
    {
        SWC_LAUNCH_REQUEST_OPEN_DEVICE,
        SWC_LAUNCH_REQUEST_ACTIVATE_VT
    } type;

    uint32_t serial;

    union
    {
        struct /* OPEN_DEVICE */
        {
            int flags;
            char path[];
        };
        struct /* ACTIVATE_VT */
        {
            unsigned vt;
        };
    };
};

struct swc_launch_event
{
    enum
    {
        SWC_LAUNCH_EVENT_RESPONSE,
        SWC_LAUNCH_EVENT_ACTIVATE,
        SWC_LAUNCH_EVENT_DEACTIVATE,
    } type;

    union
    {
        struct /* RESPONSE */
        {
            uint32_t serial;
            bool success;
        };
    };
};

ssize_t send_fd(int socket, int fd, const void * buffer, ssize_t buffer_size);

ssize_t receive_fd(int socket, int * fd, void * buffer,
                   ssize_t buffer_size);

#endif

