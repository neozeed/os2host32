#include <stddef.h>
#include <string.h>

#include "os2_queue.h"
#include "os2_queue_backend.h"

static int queue_ready(const struct Os2QueueSession *session)
{
    return session != NULL && session->backend != NULL &&
           session->backend->lock != NULL &&
           session->backend->unlock != NULL &&
           session->backend->current_pid != NULL &&
           session->backend->create_availability != NULL &&
           session->backend->destroy_availability != NULL &&
           session->backend->set_available != NULL &&
           session->backend->wait_available != NULL;
}

static unsigned char ascii_fold(unsigned char c)
{
    if (c >= (unsigned char)'a' && c <= (unsigned char)'z')
        return (unsigned char)(c - (unsigned char)'a' + (unsigned char)'A');
    return c;
}

static int queue_name_equal(const char *a, const char *b)
{
    unsigned char ca;
    unsigned char cb;
    if (a == NULL || b == NULL)
        return 0;
    do {
        ca = ascii_fold((unsigned char)*a++);
        cb = ascii_fold((unsigned char)*b++);
        if (ca != cb)
            return 0;
    } while (ca != 0U);
    return 1;
}

static int queue_name_valid(const char *name)
{
    static const char prefix[] = "\\QUEUES\\";
    size_t i;
    size_t n;

    if (name == NULL)
        return 0;
    for (i = 0U; prefix[i] != '\0'; ++i) {
        if (name[i] == '\0' ||
            ascii_fold((unsigned char)name[i]) !=
            ascii_fold((unsigned char)prefix[i]))
            return 0;
    }
    if (name[i] == '\0')
        return 0;
    n = strlen(name);
    if (n >= (size_t)OS2_QUEUE_NAME_MAX)
        return 0;
    return 1;
}

static int queue_flags_valid(Os2QueueU32 flags)
{
    if ((flags & ~OS2_QUEUE_VALID_FLAGS) != 0UL)
        return 0;
    return (flags & OS2_QUEUE_ORDER_MASK) <= OS2_QUEUE_PRIORITY;
}

static struct Os2QueueObject *queue_from_handle_locked(
    struct Os2QueueSession *session, Os2QueueHandle hq)
{
    Os2QueueU32 index;
    if (hq == 0UL)
        return NULL;
    index = hq - 1UL;
    if (index >= (Os2QueueU32)OS2_QUEUE_MAX_QUEUES)
        return NULL;
    if (!session->queues[index].used)
        return NULL;
    return &session->queues[index];
}

static struct Os2QueueObject *find_queue_locked(struct Os2QueueSession *session,
                                                const char *name,
                                                Os2QueueHandle *handle)
{
    unsigned int i;
    for (i = 0U; i < OS2_QUEUE_MAX_QUEUES; ++i) {
        if (session->queues[i].used &&
            queue_name_equal(session->queues[i].name, name)) {
            if (handle != NULL)
                *handle = (Os2QueueHandle)(i + 1U);
            return &session->queues[i];
        }
    }
    return NULL;
}

static void remove_entry(struct Os2QueueObject *queue, Os2QueueU32 index)
{
    Os2QueueU32 tail_count;
    if (index >= queue->count)
        return;
    tail_count = queue->count - index - 1UL;
    if (tail_count != 0UL) {
        memmove(&queue->entries[index], &queue->entries[index + 1UL],
                (size_t)tail_count * sizeof(queue->entries[0]));
    }
    --queue->count;
}

static Os2QueueU32 insert_position(const struct Os2QueueObject *queue,
                                   unsigned char priority)
{
    Os2QueueU32 i;
    switch (queue->discipline & OS2_QUEUE_ORDER_MASK) {
    case OS2_QUEUE_LIFO:
        return 0UL;
    case OS2_QUEUE_PRIORITY:
        for (i = 0UL; i < queue->count; ++i) {
            if (queue->entries[i].priority < priority)
                return i;
        }
        return queue->count;
    case OS2_QUEUE_FIFO:
    default:
        return queue->count;
    }
}

static Os2QueueApiRet try_read_locked(struct Os2QueueSession *session,
                                      Os2QueueHandle hq,
                                      struct Os2QueueRequestData *request,
                                      Os2QueueU32 *length,
                                      Os2QueueU32 *data_value,
                                      Os2QueueU32 element,
                                      unsigned char *priority)
{
    struct Os2QueueObject *queue;
    struct Os2QueueEntry entry;
    Os2QueueU32 pid;

    queue = queue_from_handle_locked(session, hq);
    if (queue == NULL)
        return OS2_QUEUE_ERROR_QUE_INVALID_HANDLE;
    pid = session->backend->current_pid(session->backend_opaque);
    if (pid != queue->owner_pid)
        return OS2_QUEUE_ERROR_QUE_PROC_NOT_OWNED;
    if (element != 0UL)
        return OS2_QUEUE_ERROR_QUE_ELEMENT_NOT_EXIST;
    if (queue->count == 0UL)
        return OS2_QUEUE_ERROR_QUE_EMPTY;

    entry = queue->entries[0];
    remove_entry(queue, 0UL);
    if (queue->count == 0UL) {
        if (!session->backend->set_available(session->backend_opaque,
                                             queue->availability, 0))
            return OS2_QUEUE_ERROR_QUE_INVALID_HANDLE;
    }

    request->pid = entry.pid;
    request->data = entry.request;
    *length = entry.length;
    *data_value = entry.data_value;
    *priority = entry.priority;
    return OS2_QUEUE_NO_ERROR;
}

void os2_queue_session_init(struct Os2QueueSession *session,
                            void *backend_opaque,
                            const struct Os2QueueBackendOps *backend)
{
    if (session == NULL)
        return;
    memset(session, 0, sizeof(*session));
    session->backend_opaque = backend_opaque;
    session->backend = backend;
}

void os2_queue_session_destroy(struct Os2QueueSession *session)
{
    unsigned int i;
    if (session == NULL)
        return;
    if (queue_ready(session)) {
        session->backend->lock(session->backend_opaque);
        for (i = 0U; i < OS2_QUEUE_MAX_QUEUES; ++i) {
            if (session->queues[i].used &&
                session->queues[i].availability != OS2_QUEUE_NATIVE_INVALID) {
                session->backend->destroy_availability(
                    session->backend_opaque, session->queues[i].availability);
            }
            memset(&session->queues[i], 0, sizeof(session->queues[i]));
        }
        session->backend->unlock(session->backend_opaque);
    }
    session->backend = NULL;
    session->backend_opaque = NULL;
}

Os2QueueApiRet os2_queue_DosCreateQueue(struct Os2QueueSession *session,
                                        Os2QueueHandle *phq,
                                        Os2QueueU32 flags,
                                        const char *name)
{
    unsigned int i;
    struct Os2QueueObject *queue;
    Os2QueueNative availability;
    size_t n;

    if (!queue_ready(session) || phq == NULL)
        return OS2_QUEUE_ERROR_INVALID_PARAMETER;
    if (!queue_name_valid(name))
        return OS2_QUEUE_ERROR_QUE_INVALID_NAME;
    if (!queue_flags_valid(flags))
        return OS2_QUEUE_ERROR_QUE_INVALID_PRIORITY;

    session->backend->lock(session->backend_opaque);
    if (find_queue_locked(session, name, NULL) != NULL) {
        session->backend->unlock(session->backend_opaque);
        return OS2_QUEUE_ERROR_QUE_DUPLICATE;
    }
    for (i = 0U; i < OS2_QUEUE_MAX_QUEUES; ++i) {
        if (!session->queues[i].used)
            break;
    }
    if (i == OS2_QUEUE_MAX_QUEUES) {
        session->backend->unlock(session->backend_opaque);
        return OS2_QUEUE_ERROR_QUE_NO_MEMORY;
    }

    availability = OS2_QUEUE_NATIVE_INVALID;
    if (!session->backend->create_availability(session->backend_opaque,
                                                &availability)) {
        session->backend->unlock(session->backend_opaque);
        return OS2_QUEUE_ERROR_QUE_NO_MEMORY;
    }

    queue = &session->queues[i];
    memset(queue, 0, sizeof(*queue));
    n = strlen(name);
    memcpy(queue->name, name, n + 1U);
    queue->discipline = flags;
    queue->owner_pid = session->backend->current_pid(session->backend_opaque);
    queue->availability = availability;
    queue->used = 1;
    *phq = (Os2QueueHandle)(i + 1U);
    session->backend->unlock(session->backend_opaque);
    return OS2_QUEUE_NO_ERROR;
}

Os2QueueApiRet os2_queue_DosOpenQueue(struct Os2QueueSession *session,
                                      Os2QueueU32 *owner_pid,
                                      Os2QueueHandle *phq,
                                      const char *name)
{
    struct Os2QueueObject *queue;
    Os2QueueHandle hq;

    if (!queue_ready(session) || owner_pid == NULL || phq == NULL)
        return OS2_QUEUE_ERROR_INVALID_PARAMETER;
    if (!queue_name_valid(name))
        return OS2_QUEUE_ERROR_QUE_NAME_NOT_EXIST;

    session->backend->lock(session->backend_opaque);
    queue = find_queue_locked(session, name, &hq);
    if (queue == NULL) {
        session->backend->unlock(session->backend_opaque);
        return OS2_QUEUE_ERROR_QUE_NAME_NOT_EXIST;
    }
    *owner_pid = queue->owner_pid;
    *phq = hq;
    session->backend->unlock(session->backend_opaque);
    return OS2_QUEUE_NO_ERROR;
}

Os2QueueApiRet os2_queue_DosWriteQueue(struct Os2QueueSession *session,
                                       Os2QueueHandle hq,
                                       Os2QueueU32 request,
                                       Os2QueueU32 length,
                                       Os2QueueU32 data_value,
                                       Os2QueueU32 priority)
{
    struct Os2QueueObject *queue;
    struct Os2QueueEntry entry;
    Os2QueueU32 pos;
    Os2QueueU32 tail_count;
    int was_empty;

    if (!queue_ready(session))
        return OS2_QUEUE_ERROR_INVALID_PARAMETER;
    if (priority > 15UL)
        return OS2_QUEUE_ERROR_QUE_INVALID_PRIORITY;

    entry.request = request;
    entry.length = length;
    entry.data_value = data_value;
    entry.priority = (unsigned char)priority;
    entry.pid = session->backend->current_pid(session->backend_opaque);

    session->backend->lock(session->backend_opaque);
    queue = queue_from_handle_locked(session, hq);
    if (queue == NULL) {
        session->backend->unlock(session->backend_opaque);
        return OS2_QUEUE_ERROR_QUE_INVALID_HANDLE;
    }
    if (queue->count >= (Os2QueueU32)OS2_QUEUE_DEPTH) {
        session->backend->unlock(session->backend_opaque);
        return OS2_QUEUE_ERROR_QUE_UNABLE_TO_ADD;
    }

    was_empty = queue->count == 0UL;
    pos = insert_position(queue, entry.priority);
    tail_count = queue->count - pos;
    if (tail_count != 0UL) {
        memmove(&queue->entries[pos + 1UL], &queue->entries[pos],
                (size_t)tail_count * sizeof(queue->entries[0]));
    }
    queue->entries[pos] = entry;
    ++queue->count;

    if (was_empty &&
        !session->backend->set_available(session->backend_opaque,
                                         queue->availability, 1)) {
        remove_entry(queue, pos);
        session->backend->unlock(session->backend_opaque);
        return OS2_QUEUE_ERROR_QUE_UNABLE_TO_ADD;
    }

    session->backend->unlock(session->backend_opaque);
    return OS2_QUEUE_NO_ERROR;
}

Os2QueueApiRet os2_queue_try_read(struct Os2QueueSession *session,
                                  Os2QueueHandle hq,
                                  struct Os2QueueRequestData *request,
                                  Os2QueueU32 *length,
                                  Os2QueueU32 *data_value,
                                  Os2QueueU32 element,
                                  unsigned char *priority)
{
    Os2QueueApiRet rc;
    if (!queue_ready(session) || request == NULL || length == NULL ||
        data_value == NULL || priority == NULL)
        return OS2_QUEUE_ERROR_INVALID_PARAMETER;
    session->backend->lock(session->backend_opaque);
    rc = try_read_locked(session, hq, request, length, data_value,
                         element, priority);
    session->backend->unlock(session->backend_opaque);
    return rc;
}

Os2QueueApiRet os2_queue_DosReadQueue(struct Os2QueueSession *session,
                                      Os2QueueHandle hq,
                                      struct Os2QueueRequestData *request,
                                      Os2QueueU32 *length,
                                      Os2QueueU32 *data_value,
                                      Os2QueueU32 element,
                                      Os2QueueU32 wait,
                                      unsigned char *priority,
                                      Os2QueueU32 hev)
{
    struct Os2QueueObject *queue;
    Os2QueueNative availability;
    Os2QueueApiRet rc;
    enum Os2QueueWaitResult wr;

    (void)hev;
    if (!queue_ready(session) || request == NULL || length == NULL ||
        data_value == NULL || priority == NULL)
        return OS2_QUEUE_ERROR_INVALID_PARAMETER;
    if (wait != OS2_QUEUE_DCWW_WAIT && wait != OS2_QUEUE_DCWW_NOWAIT)
        return OS2_QUEUE_ERROR_QUE_INVALID_WAIT;
    if (element != 0UL)
        return OS2_QUEUE_ERROR_QUE_ELEMENT_NOT_EXIST;

    for (;;) {
        session->backend->lock(session->backend_opaque);
        queue = queue_from_handle_locked(session, hq);
        if (queue == NULL) {
            session->backend->unlock(session->backend_opaque);
            return OS2_QUEUE_ERROR_QUE_INVALID_HANDLE;
        }
        if (session->backend->current_pid(session->backend_opaque) !=
            queue->owner_pid) {
            session->backend->unlock(session->backend_opaque);
            return OS2_QUEUE_ERROR_QUE_PROC_NOT_OWNED;
        }
        if (queue->count != 0UL) {
            rc = try_read_locked(session, hq, request, length, data_value,
                                 element, priority);
            session->backend->unlock(session->backend_opaque);
            return rc;
        }
        availability = queue->availability;
        session->backend->unlock(session->backend_opaque);

        if (wait == OS2_QUEUE_DCWW_NOWAIT)
            return OS2_QUEUE_ERROR_QUE_EMPTY;
        wr = session->backend->wait_available(session->backend_opaque,
                                              availability, 1);
        if (wr == OS2_QUEUE_WAIT_FAILED)
            return OS2_QUEUE_ERROR_QUE_INVALID_HANDLE;
        if (wr == OS2_QUEUE_WAIT_EMPTY)
            return OS2_QUEUE_ERROR_QUE_EMPTY;
        /* READY is only a wake indication.  Recheck common state under lock. */
    }
}

Os2QueueU32 os2_queue_count(struct Os2QueueSession *session,
                            Os2QueueHandle hq)
{
    struct Os2QueueObject *queue;
    Os2QueueU32 count;
    if (!queue_ready(session))
        return 0UL;
    session->backend->lock(session->backend_opaque);
    queue = queue_from_handle_locked(session, hq);
    count = queue != NULL ? queue->count : 0UL;
    session->backend->unlock(session->backend_opaque);
    return count;
}
