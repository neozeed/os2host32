#ifndef SOFT386_QUEUE_BRIDGE_H
#define SOFT386_QUEUE_BRIDGE_H
#include "soft386_system_bridge.h"
#define S386_QUEUE_HANDLES 16
struct Soft386QueueBridge {
    struct Soft386NativeModule module;
    const char *path;
    uint32_t native[S386_QUEUE_HANDLES], pid;
    int disabled, trace, waiting;
};
uint32_t soft386_queue_named_ordinal(const char *);
int soft386_queue_export(struct Soft386QueueBridge *,uint32_t ordinal);
uint32_t soft386_queue_dispatch(struct Soft386QueueBridge *,const struct Soft386GuestMemoryOps *,uint32_t ordinal,uint32_t esp,int *handled);
void soft386_queue_close(struct Soft386QueueBridge *);
#endif
