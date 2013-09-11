#ifndef SWC_UTIL_H
#define SWC_UTIL_H 1

#include <stdbool.h>
#include <sys/param.h>
#include <wayland-server.h>
#include <pixman.h>

#ifndef offsetof
#   define offsetof __builtin_offsetof
#endif

#define swc_container_of(ptr, type, member) ({                              \
    const typeof(((type *) 0)->member) *__mptr = (ptr);                     \
    ((type *) ((uintptr_t) __mptr - offsetof(type, member)));               \
})

void swc_remove_resource(struct wl_resource * resource);

static inline bool swc_rectangle_overlap
    (pixman_rectangle32_t * r1, pixman_rectangle32_t * r2)
{
    return (MAX(r1->x + r1->width, r2->x + r2->width) - MIN(r1->x, r2->x)
            < r1->width + r2->width)
        && (MAX(r1->y + r1->height, r2->y + r2->height) - MIN(r1->y, r2->y)
            < r1->height + r2->height);
}

int swc_launch_open_input_device(int socket, const char * path, int flags);
bool swc_launch_drm_master(int socket, int fd, bool set);

/* Double Buffers */
struct swc_double_buffer
{
    void * buffers[2];
    uint8_t front;
};

static inline void * swc_double_buffer_front
    (struct swc_double_buffer * double_buffer)
{
    return double_buffer->buffers[double_buffer->front];
}

static inline void * swc_double_buffer_back
    (struct swc_double_buffer * double_buffer)
{
    return double_buffer->buffers[double_buffer->front ^ 1];
}

static inline void swc_double_buffer_swap
    (struct swc_double_buffer * double_buffer)
{
    double_buffer->front ^= 1;
}

#endif

