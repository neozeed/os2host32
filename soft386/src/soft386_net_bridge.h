#ifndef SOFT386_NET_BRIDGE_H
#define SOFT386_NET_BRIDGE_H
#include "soft386_doscalls_bridge.h"
#define S386_NET_THREADS 32
#define S386_NET_ARENA 65536u
struct Soft386NetJob;
struct Soft386NetBridge {
    struct Soft386NativeDoscalls modules[2];
    const char *paths[2];
    struct Soft386NetJob *jobs[S386_NET_THREADS];
    uint32_t tids[S386_NET_THREADS];
    int errors[S386_NET_THREADS],herrors[S386_NET_THREADS];
    int sockets[256], socket_used[256];
    uint32_t socket_generation[256];
    int disabled, trace, waiting, pending_on_close;
};
uint32_t soft386_net_named_ordinal(unsigned module,const char *name);
uint32_t soft386_net_dispatch(struct Soft386NetBridge *,const struct Soft386GuestMemoryOps *,unsigned module,uint32_t ordinal,uint32_t esp,unsigned slot,uint32_t tid,uint32_t arena,int *handled);
void soft386_net_close(struct Soft386NetBridge *);
void soft386_net_quiesce_process(struct Soft386NetBridge *);
#endif
