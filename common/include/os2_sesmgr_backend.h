#ifndef OS2_SESMGR_BACKEND_H
#define OS2_SESMGR_BACKEND_H

#include "os2_sesmgr.h"

/* Host/runtime contract for the backend-neutral Session Manager core.
 * The registry returned by registry_lock is storage only; the common layer
 * owns its format and all ownership/title/query/reap semantics.
 */
struct Os2SesmgrBackendOps {
    void (*local_lock)(void *opaque);
    void (*local_unlock)(void *opaque);

    int (*registry_lock)(void *opaque, struct Os2SesmgrRegistry **registry);
    void (*registry_unlock)(void *opaque);

    int (*current_identity)(void *opaque, struct Os2SesmgrIdentity *identity);
    int (*identity_from_process)(void *opaque, Os2SesmgrNative process,
                                 Os2SesmgrU32 pid,
                                 struct Os2SesmgrIdentity *identity);
    int (*identity_alive)(void *opaque,
                          const struct Os2SesmgrIdentity *identity);
    int (*owned_process_alive)(void *opaque, Os2SesmgrNative process);

    Os2SesmgrApiRet (*launch)(void *opaque,
                              struct Os2SesmgrStartRequest *request,
                              struct Os2SesmgrLaunch *launch);
    Os2SesmgrApiRet (*resume)(void *opaque, Os2SesmgrNative initial_thread);
    void (*close_thread)(void *opaque, Os2SesmgrNative initial_thread);
    Os2SesmgrApiRet (*terminate)(void *opaque, Os2SesmgrNative process,
                                 Os2SesmgrNative group);
    void (*wait_process)(void *opaque, Os2SesmgrNative process);
    void (*close_process)(void *opaque, Os2SesmgrNative process,
                          Os2SesmgrNative group);
    void (*set_current_title)(void *opaque, const char *title);
    void (*trace_start)(void *opaque, Os2SesmgrU32 session_id,
                        Os2SesmgrU32 pid);
    void (*trace_stop)(void *opaque, Os2SesmgrU32 session_id,
                       Os2SesmgrU32 pid);
};

#endif
