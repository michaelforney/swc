#ifndef SWC_UTIL_H
#define SWC_UTIL_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <sys/param.h>
#include <pixman.h>
#include <wayland-util.h>

#define EXPORT __attribute__((visibility("default")))

#if ENABLE_DEBUG
# define MESSAGE_SOURCE \
    fprintf(stderr, "[swc:%s:%d] ", __FILE__, __LINE__);
#else
# define MESSAGE_SOURCE
#endif

#define MESSAGE(type, format, ...) \
    do { MESSAGE_SOURCE                                         \
         fprintf(stderr, type ": " format, ## __VA_ARGS__); }   \
    while (false)

#define WARNING(format, ...)    MESSAGE("WARNING", format, ## __VA_ARGS__)
#define ERROR(format, ...)      MESSAGE("ERROR", format, ## __VA_ARGS__)

#if ENABLE_DEBUG
# define DEBUG(format, ...)     MESSAGE("DEBUG", format, ## __VA_ARGS__)
#else
# define DEBUG(format, ...)
#endif

#ifdef offsetof
# define OFFSET_OF offsetof
#else
# define OFFSET_OF __builtin_offsetof
#endif

#define CONTAINER_OF(ptr, type, member) ({                                  \
    const typeof(((type *) 0)->member) *__mptr = (ptr);                     \
    ((type *) ((uintptr_t) __mptr - OFFSET_OF(type, member)));              \
})

struct wl_resource;

void swc_remove_resource(struct wl_resource * resource);

static inline uint32_t swc_time()
{
    struct timeval timeval;

    gettimeofday(&timeval, NULL);
    return timeval.tv_sec * 1000 + timeval.tv_usec / 1000;
}

static inline bool swc_rectangle_contains_point
    (pixman_rectangle32_t * rectangle, int32_t x, int32_t y)
{
    return x > rectangle->x && x < rectangle->x + rectangle->width
        && y > rectangle->y && y < rectangle->y + rectangle->height;
}

static inline bool swc_rectangle_overlap
    (pixman_rectangle32_t * r1, pixman_rectangle32_t * r2)
{
    return (MAX(r1->x + r1->width, r2->x + r2->width) - MIN(r1->x, r2->x)
            < r1->width + r2->width)
        && (MAX(r1->y + r1->height, r2->y + r2->height) - MIN(r1->y, r2->y)
            < r1->height + r2->height);
}

static inline void swc_array_remove(struct wl_array * array,
                                    void * item, size_t size)
{
    size_t bytes = array->size - ((intptr_t) item + size - (intptr_t) array->data);
    if (bytes > 0)
        memmove(item, (void *)((intptr_t) item + size), bytes);
    array->size -= size;
}

/* Launch Utilities */
int swc_launch_open_device(const char * path, int flags);
bool swc_launch_activate_vt(unsigned vt);

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

