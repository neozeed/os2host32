#ifndef OS2_QUEUE_BACKEND_H
#define OS2_QUEUE_BACKEND_H

#include "os2_queue.h"

enum Os2QueueWaitResult {
    OS2_QUEUE_WAIT_READY = 0,
    OS2_QUEUE_WAIT_EMPTY = 1,
    OS2_QUEUE_WAIT_FAILED = 2
};

struct Os2QueueBackendOps {
    void (*lock)(void *opaque);
    void (*unlock)(void *opaque);
    Os2QueueU32 (*current_pid)(void *opaque);

    int (*create_availability)(void *opaque, Os2QueueNative *token);
    void (*destroy_availability)(void *opaque, Os2QueueNative token);
    int (*set_available)(void *opaque, Os2QueueNative token, int available);
    enum Os2QueueWaitResult (*wait_available)(void *opaque,
                                               Os2QueueNative token,
                                               int wait);
};

#endif
