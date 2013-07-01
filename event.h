#ifndef SWC_EVENT_H
#define SWC_EVENT_H

#include <stdint.h>

struct swc_event
{
    uint32_t type;
    void * data;
};

#endif

