#include <stdio.h>
#include <string.h>

#include "os2_queue.h"
#include "os2_queue_backend.h"

struct FakeBackend {
    Os2QueueU32 pid;
    unsigned int lock_depth;
    Os2QueueNative next_token;
    int available[64];
    unsigned int set_calls;
    unsigned int wait_calls;
};

static int failures;

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static void fake_lock(void *opaque)
{
    struct FakeBackend *f = (struct FakeBackend *)opaque;
    ++f->lock_depth;
}

static void fake_unlock(void *opaque)
{
    struct FakeBackend *f = (struct FakeBackend *)opaque;
    if (f->lock_depth != 0U)
        --f->lock_depth;
}

static Os2QueueU32 fake_pid(void *opaque)
{
    return ((struct FakeBackend *)opaque)->pid;
}

static int fake_create(void *opaque, Os2QueueNative *token)
{
    struct FakeBackend *f = (struct FakeBackend *)opaque;
    ++f->next_token;
    if (f->next_token >= (Os2QueueNative)64U)
        return 0;
    *token = f->next_token;
    f->available[(unsigned int)*token] = 0;
    return 1;
}

static void fake_destroy(void *opaque, Os2QueueNative token)
{
    struct FakeBackend *f = (struct FakeBackend *)opaque;
    if (token < (Os2QueueNative)64U)
        f->available[(unsigned int)token] = 0;
}

static int fake_set(void *opaque, Os2QueueNative token, int available)
{
    struct FakeBackend *f = (struct FakeBackend *)opaque;
    ++f->set_calls;
    if (token == 0U || token >= (Os2QueueNative)64U)
        return 0;
    f->available[(unsigned int)token] = available ? 1 : 0;
    return 1;
}

static enum Os2QueueWaitResult fake_wait(void *opaque, Os2QueueNative token,
                                         int wait)
{
    struct FakeBackend *f = (struct FakeBackend *)opaque;
    (void)wait;
    ++f->wait_calls;
    if (token == 0U || token >= (Os2QueueNative)64U)
        return OS2_QUEUE_WAIT_FAILED;
    return f->available[(unsigned int)token] ?
           OS2_QUEUE_WAIT_READY : OS2_QUEUE_WAIT_EMPTY;
}

static const struct Os2QueueBackendOps fake_ops = {
    fake_lock,
    fake_unlock,
    fake_pid,
    fake_create,
    fake_destroy,
    fake_set,
    fake_wait
};

static Os2QueueApiRet read_nowait(struct Os2QueueSession *s, Os2QueueHandle hq,
                                  Os2QueueU32 *request_code,
                                  Os2QueueU32 *data_value,
                                  unsigned char *priority)
{
    struct Os2QueueRequestData request;
    Os2QueueU32 length;
    Os2QueueApiRet rc;
    memset(&request, 0, sizeof(request));
    length = 0UL;
    *data_value = 0UL;
    *priority = 0U;
    rc = os2_queue_DosReadQueue(s, hq, &request, &length, data_value,
                                0UL, OS2_QUEUE_DCWW_NOWAIT, priority, 0UL);
    if (rc == 0UL)
        *request_code = request.data;
    else
        *request_code = 0UL;
    return rc;
}

int main(void)
{
    struct FakeBackend backend;
    struct Os2QueueSession session;
    struct Os2QueueRequestData request;
    Os2QueueHandle fifo;
    Os2QueueHandle lifo;
    Os2QueueHandle priority_q;
    Os2QueueHandle convert_q;
    Os2QueueHandle opened;
    Os2QueueU32 owner;
    Os2QueueU32 req;
    Os2QueueU32 data;
    Os2QueueU32 length;
    unsigned char pri;
    Os2QueueApiRet rc;

    memset(&backend, 0, sizeof(backend));
    backend.pid = 1234UL;
    os2_queue_session_init(&session, &backend, &fake_ops);

    fifo = 0UL;
    check(os2_queue_DosCreateQueue(&session, &fifo, OS2_QUEUE_FIFO,
                                   "\\QUEUES\\fifo.que") == 0UL,
          "create FIFO queue");
    check(fifo != 0UL, "FIFO handle assigned");
    check(os2_queue_DosCreateQueue(&session, &opened, OS2_QUEUE_FIFO,
                                   "\\queues\\FIFO.QUE") ==
                                   OS2_QUEUE_ERROR_QUE_DUPLICATE,
          "queue names are case insensitive");
    check(os2_queue_DosCreateQueue(&session, &opened, OS2_QUEUE_FIFO,
                                   "fifo.que") ==
                                   OS2_QUEUE_ERROR_QUE_INVALID_NAME,
          "queue name requires \\QUEUES\\ prefix");
    check(os2_queue_DosCreateQueue(&session, &opened, 3UL,
                                   "\\QUEUES\\badorder") ==
                                   OS2_QUEUE_ERROR_QUE_INVALID_PRIORITY,
          "invalid queue discipline rejected");

    owner = 0UL;
    opened = 0UL;
    check(os2_queue_DosOpenQueue(&session, &owner, &opened,
                                 "\\queues\\FIFO.que") == 0UL,
          "open queue case insensitively");
    check(opened == fifo && owner == 1234UL,
          "open returns handle and creating PID");

    check(os2_queue_DosWriteQueue(&session, fifo, 10UL, 4UL,
                                  0x11111111UL, 1UL) == 0UL,
          "FIFO write 1");
    check(os2_queue_DosWriteQueue(&session, fifo, 20UL, 8UL,
                                  0xdeadbeefUL, 2UL) == 0UL,
          "FIFO write 2 opaque address");
    check(os2_queue_count(&session, fifo) == 2UL, "FIFO count after writes");

    req = data = 0UL; pri = 0U;
    check(read_nowait(&session, fifo, &req, &data, &pri) == 0UL &&
          req == 10UL && data == 0x11111111UL && pri == 1U,
          "FIFO reads first entry first");
    req = data = 0UL; pri = 0U;
    check(read_nowait(&session, fifo, &req, &data, &pri) == 0UL &&
          req == 20UL && data == 0xdeadbeefUL && pri == 2U,
          "FIFO preserves opaque data value");
    check(read_nowait(&session, fifo, &req, &data, &pri) ==
          OS2_QUEUE_ERROR_QUE_EMPTY,
          "NOWAIT empty returns ERROR_QUE_EMPTY");

    lifo = 0UL;
    check(os2_queue_DosCreateQueue(&session, &lifo, OS2_QUEUE_LIFO,
                                   "\\QUEUES\\lifo") == 0UL,
          "create LIFO queue");
    check(os2_queue_DosWriteQueue(&session, lifo, 1UL, 0UL, 1UL, 0UL) == 0UL,
          "LIFO write 1");
    check(os2_queue_DosWriteQueue(&session, lifo, 2UL, 0UL, 2UL, 0UL) == 0UL,
          "LIFO write 2");
    check(read_nowait(&session, lifo, &req, &data, &pri) == 0UL && req == 2UL,
          "LIFO reads newest entry first");

    priority_q = 0UL;
    check(os2_queue_DosCreateQueue(&session, &priority_q, OS2_QUEUE_PRIORITY,
                                   "\\QUEUES\\priority") == 0UL,
          "create priority queue");
    check(os2_queue_DosWriteQueue(&session, priority_q, 1UL, 0UL, 1UL, 5UL) == 0UL,
          "priority write A");
    check(os2_queue_DosWriteQueue(&session, priority_q, 2UL, 0UL, 2UL, 5UL) == 0UL,
          "priority write equal B");
    check(os2_queue_DosWriteQueue(&session, priority_q, 3UL, 0UL, 3UL, 15UL) == 0UL,
          "priority write high C");
    check(read_nowait(&session, priority_q, &req, &data, &pri) == 0UL && req == 3UL,
          "priority queue returns highest priority first");
    check(read_nowait(&session, priority_q, &req, &data, &pri) == 0UL && req == 1UL,
          "equal priority remains FIFO A");
    check(read_nowait(&session, priority_q, &req, &data, &pri) == 0UL && req == 2UL,
          "equal priority remains FIFO B");

    convert_q = 0UL;
    check(os2_queue_DosCreateQueue(&session, &convert_q,
                                   OS2_QUEUE_FIFO | OS2_QUEUE_CONVERT_ADDRESS,
                                   "\\QUEUES\\convert") == 0UL,
          "QUE_CONVERT_ADDRESS accepted for 32-bit queue");
    check(os2_queue_DosWriteQueue(&session, convert_q, 1UL, 0UL, 0UL, 16UL) ==
          OS2_QUEUE_ERROR_QUE_INVALID_PRIORITY,
          "priority above 15 rejected");

    memset(&request, 0, sizeof(request));
    length = 0UL;
    data = 0UL;
    pri = 0U;
    rc = os2_queue_DosReadQueue(&session, convert_q, &request, &length, &data,
                                0UL, 7UL, &pri, 0UL);
    check(rc == OS2_QUEUE_ERROR_QUE_INVALID_WAIT,
          "invalid wait flag rejected");
    rc = os2_queue_DosReadQueue(&session, convert_q, &request, &length, &data,
                                99UL, OS2_QUEUE_DCWW_NOWAIT, &pri, 0UL);
    check(rc == OS2_QUEUE_ERROR_QUE_ELEMENT_NOT_EXIST,
          "nonzero element unsupported without Peek token");

    check(os2_queue_DosWriteQueue(&session, convert_q, 55UL, 9UL,
                                  0x12345678UL, 0UL) == 0UL,
          "owner test write");
    backend.pid = 4321UL;
    rc = os2_queue_DosReadQueue(&session, convert_q, &request, &length, &data,
                                0UL, OS2_QUEUE_DCWW_NOWAIT, &pri, 0UL);
    check(rc == OS2_QUEUE_ERROR_QUE_PROC_NOT_OWNED,
          "only owner process can read");
    backend.pid = 1234UL;
    rc = os2_queue_DosReadQueue(&session, convert_q, &request, &length, &data,
                                0UL, OS2_QUEUE_DCWW_WAIT, &pri, 0UL);
    check(rc == 0UL && request.pid == 1234UL && request.data == 55UL &&
          length == 9UL && data == 0x12345678UL,
          "WAIT read consumes already available entry without host wait");

    check(backend.lock_depth == 0U, "locks balanced");
    check(backend.set_calls != 0U, "availability backend exercised");
    check(backend.wait_calls == 0U,
          "no backend wait needed when tests have data or NOWAIT");

    os2_queue_session_destroy(&session);
    check(backend.lock_depth == 0U, "destroy leaves lock balanced");

    if (failures != 0) {
        fprintf(stderr, "queue-core-check: %d failure(s)\n", failures);
        return 1;
    }
    printf("queue-core-check: PASS\n");
    return 0;
}
