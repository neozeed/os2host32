#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "os2_queue.h"
#include "os2_queue_backend.h"
#include "os2_queue_win32.h"

#ifndef __cdecl
#define __cdecl
#endif

static struct Os2QueueSession session;
static int available[32];
static Os2QueueNative next_token;
static int failures;

static void lock_noop(void *opaque) { (void)opaque; }
static void unlock_noop(void *opaque) { (void)opaque; }
static Os2QueueU32 fake_pid(void *opaque) { (void)opaque; return 77UL; }
static int fake_create(void *opaque, Os2QueueNative *token)
{
    (void)opaque;
    ++next_token;
    *token = next_token;
    available[(unsigned int)*token] = 0;
    return 1;
}
static void fake_destroy(void *opaque, Os2QueueNative token)
{
    (void)opaque;
    available[(unsigned int)token] = 0;
}
static int fake_set(void *opaque, Os2QueueNative token, int value)
{
    (void)opaque;
    available[(unsigned int)token] = value;
    return 1;
}
static enum Os2QueueWaitResult fake_wait(void *opaque, Os2QueueNative token,
                                         int wait)
{
    (void)opaque; (void)wait;
    return available[(unsigned int)token] ? OS2_QUEUE_WAIT_READY :
                                            OS2_QUEUE_WAIT_EMPTY;
}
static const struct Os2QueueBackendOps ops = {
    lock_noop, unlock_noop, fake_pid, fake_create, fake_destroy,
    fake_set, fake_wait
};

struct Os2QueueSession *os2_queue_win32_session(void)
{
    return &session;
}
void os2_queue_win32_session_init(struct Os2QueueSession *unused)
{
    (void)unused;
}

Os2QueueApiRet __cdecl DosCreateQueue(Os2QueueHandle *, Os2QueueU32, const char *);
Os2QueueApiRet __cdecl DosOpenQueue(Os2QueueU32 *, Os2QueueHandle *, const char *);
Os2QueueApiRet __cdecl DosWriteQueue(Os2QueueHandle, Os2QueueU32, Os2QueueU32,
                                     void *, Os2QueueU32);
Os2QueueApiRet __cdecl DosReadQueue(Os2QueueHandle, struct Os2QueueRequestData *,
                                    Os2QueueU32 *, void **, Os2QueueU32,
                                    Os2QueueU32, unsigned char *, Os2QueueU32);

static void check(int ok, const char *message)
{
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

int main(void)
{
    Os2QueueHandle hq;
    Os2QueueHandle opened;
    Os2QueueU32 owner;
    Os2QueueU32 cb;
    struct Os2QueueRequestData request;
    void *data;
    unsigned char priority;

    memset(available, 0, sizeof(available));
    next_token = 0U;
    os2_queue_session_init(&session, NULL, &ops);

    hq = 0UL;
    check(DosCreateQueue(&hq, OS2_QUEUE_FIFO, "\\QUEUES\\veneer") == 0UL,
          "veneer create routes to common QUECALLS");
    opened = 0UL;
    owner = 0UL;
    check(DosOpenQueue(&owner, &opened, "\\queues\\VENEER") == 0UL &&
          opened == hq && owner == 77UL,
          "veneer open routes and returns owner");
    check(DosWriteQueue(hq, 99UL, 4UL, (void *)(uintptr_t)440UL, 7UL) == 0UL,
          "veneer write accepts pointer-as-value payload");

    memset(&request, 0, sizeof(request));
    cb = 0UL;
    data = NULL;
    priority = 0U;
    check(DosReadQueue(hq, &request, &cb, &data, 0UL,
                       OS2_QUEUE_DCWW_NOWAIT, &priority, 0UL) == 0UL,
          "veneer read routes to common QUECALLS");
    check(request.pid == 77UL && request.data == 99UL && cb == 4UL &&
          (uintptr_t)data == (uintptr_t)440UL && priority == 7U,
          "veneer preserves returned queue fields and opaque pointer value");

    os2_queue_session_destroy(&session);
    if (failures != 0) {
        fprintf(stderr, "queue-veneer-check: %d failure(s)\n", failures);
        return 1;
    }
    printf("queue-veneer-check: PASS\n");
    return 0;
}
