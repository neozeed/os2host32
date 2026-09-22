/*
 * quecalls.c - minimal Win32 personality for OS/2 32-bit QUECALLS.DLL.
 *
 * Milestone 14 implements exactly the queue subset imported by the newer
 * Sarien PM specimen:
 *
 *      9  DosReadQueue
 *     14  DosWriteQueue
 *     15  DosOpenQueue
 *     16  DosCreateQueue
 *
 * The implementation is deliberately process-local.  That is sufficient for
 * Sarien, which creates a named FIFO queue in its main thread, opens it from a
 * beeper thread, and exchanges queue elements between threads in one process.
 *
 * Queue data addresses are retained as opaque pointer values instead of being
 * copied.  This mirrors the address-oriented OS/2 queue interface and is also
 * important for the historical Sarien sound hack, which intentionally passes
 * integer tone values through the queue's data-address field.
 *
 * ANSI C89 / old Win32 SDK friendly by design.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdio.h>

#ifndef __cdecl
#define __cdecl
#endif

typedef DWORD O2APIRET;
typedef DWORD O2ULONG;
typedef DWORD O2HQUEUE;

typedef struct O2REQUESTDATA {
    O2ULONG pid;
    O2ULONG data;
} O2REQUESTDATA;

#define O2_NO_ERROR                    0UL
#define O2_ERROR_INVALID_PARAMETER    87UL
#define O2_ERROR_QUE_DUPLICATE       332UL
#define O2_ERROR_QUE_ELEMENT_NOT_EXIST 333UL
#define O2_ERROR_QUE_NO_MEMORY       334UL
#define O2_ERROR_QUE_INVALID_HANDLE  337UL
#define O2_ERROR_QUE_EMPTY           342UL
#define O2_ERROR_QUE_NAME_NOT_EXIST  343UL
#define O2_ERROR_QUE_UNABLE_TO_ADD   346UL
#define O2_ERROR_QUE_INVALID_WAIT    433UL

#define O2_DCWW_WAIT                   0UL
#define O2_DCWW_NOWAIT                 1UL

#define O2_MAX_QUEUES                  16
#define O2_QUEUE_DEPTH                256
#define O2_QUEUE_NAME_MAX             260

struct O2QueueEntry {
    O2ULONG request;
    O2ULONG length;
    void *data;
    unsigned char priority;
    O2ULONG pid;
};

struct O2Queue {
    int used;
    char name[O2_QUEUE_NAME_MAX];
    O2ULONG discipline;
    CRITICAL_SECTION lock;
    HANDLE available;
    O2ULONG head;
    O2ULONG tail;
    O2ULONG count;
    struct O2QueueEntry entries[O2_QUEUE_DEPTH];
};

static struct O2Queue queues[O2_MAX_QUEUES];

static int audio_trace_enabled(void)
{
    char b[4];
    return GetEnvironmentVariableA("LE2PE_TRACE_AUDIO", b, sizeof(b)) != 0;
}


static struct O2Queue *queue_from_handle(O2HQUEUE hq)
{
    O2ULONG i;
    if (hq == 0UL)
        return NULL;
    i = hq - 1UL;
    if (i >= O2_MAX_QUEUES || !queues[i].used)
        return NULL;
    return &queues[i];
}

static struct O2Queue *find_queue(const char *name, O2HQUEUE *phq)
{
    O2ULONG i;
    if (!name)
        return NULL;
    for (i = 0UL; i < O2_MAX_QUEUES; ++i) {
        if (queues[i].used && lstrcmpiA(queues[i].name, name) == 0) {
            if (phq)
                *phq = i + 1UL;
            return &queues[i];
        }
    }
    return NULL;
}

/* QUECALLS.16 */
O2APIRET __cdecl DosCreateQueue(O2HQUEUE *phq, O2ULONG priority,
                                const char *name)
{
    O2ULONG i;
    struct O2Queue *q;

    if (!phq || !name || !*name)
        return O2_ERROR_INVALID_PARAMETER;
    if (find_queue(name, NULL) != NULL)
        return O2_ERROR_QUE_DUPLICATE;

    for (i = 0UL; i < O2_MAX_QUEUES; ++i) {
        if (!queues[i].used)
            break;
    }
    if (i == O2_MAX_QUEUES)
        return O2_ERROR_QUE_NO_MEMORY;

    q = &queues[i];
    memset(q, 0, sizeof(*q));
    lstrcpynA(q->name, name, O2_QUEUE_NAME_MAX);
    q->discipline = priority;
    InitializeCriticalSection(&q->lock);
    q->available = CreateSemaphoreA(NULL, 0, O2_QUEUE_DEPTH, NULL);
    if (!q->available) {
        DeleteCriticalSection(&q->lock);
        memset(q, 0, sizeof(*q));
        return O2_ERROR_QUE_NO_MEMORY;
    }

    /* Publish only after the object is fully initialized. */
    q->used = 1;
    *phq = i + 1UL;
    if (audio_trace_enabled()) {
        fprintf(stderr, "QUECALLS: DosCreateQueue name=%s flags=%lu -> hq=%lu\n",
                name, (unsigned long)priority, (unsigned long)*phq);
        fflush(stderr);
    }
    return O2_NO_ERROR;
}

/* QUECALLS.15 */
O2APIRET __cdecl DosOpenQueue(O2ULONG *pOwnerPid, O2HQUEUE *phq,
                              const char *name)
{
    O2HQUEUE hq;

    if (!pOwnerPid || !phq || !name)
        return O2_ERROR_INVALID_PARAMETER;
    if (find_queue(name, &hq) == NULL)
        return O2_ERROR_QUE_NAME_NOT_EXIST;

    *pOwnerPid = (O2ULONG)GetCurrentProcessId();
    *phq = hq;
    if (audio_trace_enabled()) {
        fprintf(stderr, "QUECALLS: DosOpenQueue name=%s -> hq=%lu owner=%lu\n",
                name, (unsigned long)hq, (unsigned long)*pOwnerPid);
        fflush(stderr);
    }
    return O2_NO_ERROR;
}

/* QUECALLS.14 */
O2APIRET __cdecl DosWriteQueue(O2HQUEUE hq, O2ULONG request,
                               O2ULONG cbData, void *pData,
                               O2ULONG priority)
{
    struct O2Queue *q;
    struct O2QueueEntry *e;

    q = queue_from_handle(hq);
    if (!q)
        return O2_ERROR_QUE_INVALID_HANDLE;
    if (priority > 15UL)
        return O2_ERROR_INVALID_PARAMETER;

    EnterCriticalSection(&q->lock);
    if (q->count >= O2_QUEUE_DEPTH) {
        LeaveCriticalSection(&q->lock);
        return O2_ERROR_QUE_UNABLE_TO_ADD;
    }

    e = &q->entries[q->tail];
    e->request = request;
    e->length = cbData;
    e->data = pData;
    e->priority = (unsigned char)priority;
    e->pid = (O2ULONG)GetCurrentProcessId();
    q->tail = (q->tail + 1UL) % O2_QUEUE_DEPTH;
    ++q->count;
    LeaveCriticalSection(&q->lock);

    if (!ReleaseSemaphore(q->available, 1, NULL))
        return O2_ERROR_QUE_UNABLE_TO_ADD;
    if (audio_trace_enabled()) {
        fprintf(stderr, "QUECALLS: DosWriteQueue hq=%lu request=%lu len=%lu data=%p pri=%lu count=%lu\n",
                (unsigned long)hq, (unsigned long)request,
                (unsigned long)cbData, pData, (unsigned long)priority,
                (unsigned long)q->count);
        fflush(stderr);
    }
    return O2_NO_ERROR;
}

/* QUECALLS.9 */
O2APIRET __cdecl DosReadQueue(O2HQUEUE hq, O2REQUESTDATA *request,
                              O2ULONG *pcbData, void **ppData,
                              O2ULONG element, O2ULONG wait,
                              unsigned char *pPriority, O2ULONG hev)
{
    struct O2Queue *q;
    struct O2QueueEntry e;
    DWORD wr;

    (void)hev;
    q = queue_from_handle(hq);
    if (!q)
        return O2_ERROR_QUE_INVALID_HANDLE;
    if (!request || !pcbData || !ppData || !pPriority)
        return O2_ERROR_INVALID_PARAMETER;
    if (element != 0UL)
        return O2_ERROR_QUE_ELEMENT_NOT_EXIST;
    if (wait != O2_DCWW_WAIT && wait != O2_DCWW_NOWAIT)
        return O2_ERROR_QUE_INVALID_WAIT;

    wr = WaitForSingleObject(q->available,
                             wait == O2_DCWW_WAIT ? INFINITE : 0UL);
    if (wr == WAIT_TIMEOUT)
        return O2_ERROR_QUE_EMPTY;
    if (wr != WAIT_OBJECT_0)
        return O2_ERROR_QUE_INVALID_HANDLE;

    EnterCriticalSection(&q->lock);
    if (q->count == 0UL) {
        /* Should not happen unless the semaphore/count relationship was
           disturbed externally; keep the API result sane. */
        LeaveCriticalSection(&q->lock);
        return O2_ERROR_QUE_EMPTY;
    }

    e = q->entries[q->head];
    q->head = (q->head + 1UL) % O2_QUEUE_DEPTH;
    --q->count;
    LeaveCriticalSection(&q->lock);

    request->pid = e.pid;
    request->data = e.request;
    *pcbData = e.length;
    *ppData = e.data;
    *pPriority = e.priority;
    if (audio_trace_enabled()) {
        fprintf(stderr, "QUECALLS: DosReadQueue hq=%lu -> request=%lu len=%lu data=%p pri=%u remain=%lu\n",
                (unsigned long)hq, (unsigned long)e.request,
                (unsigned long)e.length, e.data, (unsigned)e.priority,
                (unsigned long)q->count);
        fflush(stderr);
    }
    return O2_NO_ERROR;
}
