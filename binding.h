#ifndef SWC_BINDING_H
#define SWC_BINDING_H 1

#include <stdint.h>
#include <linux/input.h>

#define MOD_CTRL    (1 << 0)
#define MOD_ALT     (1 << 1)
#define MOD_SUPER   (1 << 2)
#define MOD_SHIFT   (1 << 3)
#define MOD_ANY     (-1)

typedef void (* swc_binding_handler_t)(uint32_t time, uint32_t value,
                                       void * data);

struct swc_binding
{
    uint32_t value;
    uint32_t modifiers;
    swc_binding_handler_t handler;
    void * data;
};

#endif

