#ifndef SWC_LAUNCH_PROTOCOL_H
#define SWC_LAUNCH_PROTOCOL_H 1

#include <stdbool.h>
#include <sys/types.h>

#define SWC_LAUNCH_SOCKET_ENV "SWC_LAUNCH_SOCKET"
#define SWC_LAUNCH_TTY_FD_ENV "SWC_LAUNCH_TTY_FD"

struct swc_launch_request
{
    enum
    {
        SWC_LAUNCH_REQUEST_DRM_MASTER,
        SWC_LAUNCH_REQUEST_OPEN_INPUT_DEVICE
    } type;
    union
    {
        struct /* DRM_MASTER */
        {
            bool set;
        };
        struct /* OPEN_INPUT_DEVICE */
        {
            int flags;
            char path[];
        };
    };
};

struct swc_launch_response
{
    bool success;
};

ssize_t send_fd(int socket, int fd, void * buffer, ssize_t buffer_size);

ssize_t receive_fd(int socket, int * fd, void * buffer,
                   ssize_t buffer_size);

#endif

