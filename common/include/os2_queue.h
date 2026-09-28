#ifndef OS2_QUEUE_H
#define OS2_QUEUE_H

/*
 * Backend-neutral OS/2 QUECALLS state and semantics.
 *
 * Queue names, handles, ordering, ownership, and queued element metadata are
 * OS/2 personality state.  Host locking, process identity and blocking/wake
 * mechanics are supplied by a backend.  Queue payload addresses are opaque
 * 32-bit OS/2 values: the common layer never dereferences or copies them.
 */

#include <stdint.h>
#include <stddef.h>

#ifndef __cdecl
#define __cdecl
#endif

typedef uint32_t Os2QueueApiRet;
typedef uint32_t Os2QueueU32;
typedef uint32_t Os2QueueHandle;
typedef uintptr_t Os2QueueNative;

#define OS2_QUEUE_NO_ERROR                     0UL
#define OS2_QUEUE_ERROR_INVALID_PARAMETER     87UL
#define OS2_QUEUE_ERROR_QUE_PROC_NOT_OWNED   330UL
#define OS2_QUEUE_ERROR_QUE_DUPLICATE        332UL
#define OS2_QUEUE_ERROR_QUE_ELEMENT_NOT_EXIST 333UL
#define OS2_QUEUE_ERROR_QUE_NO_MEMORY        334UL
#define OS2_QUEUE_ERROR_QUE_INVALID_NAME     335UL
#define OS2_QUEUE_ERROR_QUE_INVALID_PRIORITY 336UL
#define OS2_QUEUE_ERROR_QUE_INVALID_HANDLE   337UL
#define OS2_QUEUE_ERROR_QUE_EMPTY            342UL
#define OS2_QUEUE_ERROR_QUE_NAME_NOT_EXIST   343UL
#define OS2_QUEUE_ERROR_QUE_UNABLE_TO_ADD    346UL
#define OS2_QUEUE_ERROR_QUE_INVALID_WAIT     433UL

#define OS2_QUEUE_DCWW_WAIT                    0UL
#define OS2_QUEUE_DCWW_NOWAIT                  1UL

#define OS2_QUEUE_FIFO                         0UL
#define OS2_QUEUE_LIFO                         1UL
#define OS2_QUEUE_PRIORITY                     2UL
#define OS2_QUEUE_CONVERT_ADDRESS              4UL
#define OS2_QUEUE_ORDER_MASK                   3UL
#define OS2_QUEUE_VALID_FLAGS                  7UL

#define OS2_QUEUE_MAX_QUEUES                   16U
#define OS2_QUEUE_DEPTH                       256U
#define OS2_QUEUE_NAME_MAX                    260U
#define OS2_QUEUE_NATIVE_INVALID ((Os2QueueNative)0)

struct Os2QueueBackendOps;

struct Os2QueueRequestData {
    Os2QueueU32 pid;
    Os2QueueU32 data;
};

struct Os2QueueEntry {
    Os2QueueU32 request;
    Os2QueueU32 length;
    Os2QueueU32 data_value;
    unsigned char priority;
    Os2QueueU32 pid;
};

struct Os2QueueObject {
    int used;
    char name[OS2_QUEUE_NAME_MAX];
    Os2QueueU32 discipline;
    Os2QueueU32 owner_pid;
    Os2QueueNative availability;
    Os2QueueU32 count;
    struct Os2QueueEntry entries[OS2_QUEUE_DEPTH];
};

struct Os2QueueSession {
    void *backend_opaque;
    const struct Os2QueueBackendOps *backend;
    struct Os2QueueObject queues[OS2_QUEUE_MAX_QUEUES];
};

void os2_queue_session_init(struct Os2QueueSession *session,
                            void *backend_opaque,
                            const struct Os2QueueBackendOps *backend);
void os2_queue_session_destroy(struct Os2QueueSession *session);

Os2QueueApiRet os2_queue_DosCreateQueue(struct Os2QueueSession *session,
                                        Os2QueueHandle *phq,
                                        Os2QueueU32 flags,
                                        const char *name);
Os2QueueApiRet os2_queue_DosOpenQueue(struct Os2QueueSession *session,
                                      Os2QueueU32 *owner_pid,
                                      Os2QueueHandle *phq,
                                      const char *name);
Os2QueueApiRet os2_queue_DosWriteQueue(struct Os2QueueSession *session,
                                       Os2QueueHandle hq,
                                       Os2QueueU32 request,
                                       Os2QueueU32 length,
                                       Os2QueueU32 data_value,
                                       Os2QueueU32 priority);
Os2QueueApiRet os2_queue_DosReadQueue(struct Os2QueueSession *session,
                                      Os2QueueHandle hq,
                                      struct Os2QueueRequestData *request,
                                      Os2QueueU32 *length,
                                      Os2QueueU32 *data_value,
                                      Os2QueueU32 element,
                                      Os2QueueU32 wait,
                                      unsigned char *priority,
                                      Os2QueueU32 hev);

/*
 * Nonblocking semantic primitive for execution engines with their own guest
 * scheduler (WHP) or request scheduler (OS2SS).  It performs the same owner,
 * handle and element checks as DosReadQueue but never blocks.
 */
Os2QueueApiRet os2_queue_try_read(struct Os2QueueSession *session,
                                  Os2QueueHandle hq,
                                  struct Os2QueueRequestData *request,
                                  Os2QueueU32 *length,
                                  Os2QueueU32 *data_value,
                                  Os2QueueU32 element,
                                  unsigned char *priority);

Os2QueueU32 os2_queue_count(struct Os2QueueSession *session,
                            Os2QueueHandle hq);

#endif
