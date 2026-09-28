#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "os2_queue.h"
#include "os2_queue_backend.h"
#include "os2_queue_win32.h"

struct Os2QueueWin32Context {
    CRITICAL_SECTION lock;
    int lock_ready;
};

static struct Os2QueueWin32Context queue_context;
static struct Os2QueueSession queue_session_storage;
static struct Os2QueueSession *queue_session;

static void win32_lock(void *opaque)
{
    struct Os2QueueWin32Context *context;
    context = (struct Os2QueueWin32Context *)opaque;
    EnterCriticalSection(&context->lock);
}

static void win32_unlock(void *opaque)
{
    struct Os2QueueWin32Context *context;
    context = (struct Os2QueueWin32Context *)opaque;
    LeaveCriticalSection(&context->lock);
}

static Os2QueueU32 win32_current_pid(void *opaque)
{
    (void)opaque;
    return (Os2QueueU32)GetCurrentProcessId();
}

static int win32_create_availability(void *opaque, Os2QueueNative *token)
{
    HANDLE event_handle;
    (void)opaque;
    if (token == NULL)
        return 0;
    event_handle = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (event_handle == NULL)
        return 0;
    *token = (Os2QueueNative)(uintptr_t)event_handle;
    return 1;
}

static void win32_destroy_availability(void *opaque, Os2QueueNative token)
{
    HANDLE event_handle;
    (void)opaque;
    event_handle = (HANDLE)(uintptr_t)token;
    if (event_handle != NULL)
        CloseHandle(event_handle);
}

static int win32_set_available(void *opaque, Os2QueueNative token,
                               int available)
{
    HANDLE event_handle;
    (void)opaque;
    event_handle = (HANDLE)(uintptr_t)token;
    if (event_handle == NULL)
        return 0;
    if (available)
        return SetEvent(event_handle) != 0;
    return ResetEvent(event_handle) != 0;
}

static enum Os2QueueWaitResult win32_wait_available(void *opaque,
                                                     Os2QueueNative token,
                                                     int wait)
{
    HANDLE event_handle;
    DWORD result;
    (void)opaque;
    event_handle = (HANDLE)(uintptr_t)token;
    if (event_handle == NULL)
        return OS2_QUEUE_WAIT_FAILED;
    result = WaitForSingleObject(event_handle, wait ? INFINITE : 0UL);
    if (result == WAIT_OBJECT_0)
        return OS2_QUEUE_WAIT_READY;
    if (result == WAIT_TIMEOUT)
        return OS2_QUEUE_WAIT_EMPTY;
    return OS2_QUEUE_WAIT_FAILED;
}

static const struct Os2QueueBackendOps win32_backend = {
    win32_lock,
    win32_unlock,
    win32_current_pid,
    win32_create_availability,
    win32_destroy_availability,
    win32_set_available,
    win32_wait_available
};

void os2_queue_win32_session_init(struct Os2QueueSession *session)
{
    if (session == NULL)
        return;
    os2_queue_session_init(session, &queue_context, &win32_backend);
    queue_session = session;
}

struct Os2QueueSession *os2_queue_win32_session(void)
{
    return queue_session;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance;
    if (reason == DLL_PROCESS_ATTACH) {
        InitializeCriticalSection(&queue_context.lock);
        queue_context.lock_ready = 1;
        os2_queue_win32_session_init(&queue_session_storage);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (reserved == NULL && queue_session != NULL)
            os2_queue_session_destroy(queue_session);
        queue_session = NULL;
        if (queue_context.lock_ready) {
            DeleteCriticalSection(&queue_context.lock);
            queue_context.lock_ready = 0;
        }
    }
    return TRUE;
}
