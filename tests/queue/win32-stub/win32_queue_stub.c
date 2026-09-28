#include <stdlib.h>
#include "windows.h"

struct StubEvent {
    int manual_reset;
    int signaled;
};

void InitializeCriticalSection(CRITICAL_SECTION *cs) { cs->held = 0; }
void DeleteCriticalSection(CRITICAL_SECTION *cs) { (void)cs; }
void EnterCriticalSection(CRITICAL_SECTION *cs) { ++cs->held; }
void LeaveCriticalSection(CRITICAL_SECTION *cs) { --cs->held; }

HANDLE CreateEventA(void *attributes, BOOL manual_reset, BOOL initial_state,
                    const char *name)
{
    struct StubEvent *event_handle;
    (void)attributes; (void)name;
    event_handle = (struct StubEvent *)malloc(sizeof(*event_handle));
    if (event_handle == NULL)
        return NULL;
    event_handle->manual_reset = manual_reset;
    event_handle->signaled = initial_state;
    return (HANDLE)event_handle;
}

BOOL CloseHandle(HANDLE handle)
{
    free(handle);
    return TRUE;
}

BOOL SetEvent(HANDLE handle)
{
    if (handle == NULL)
        return FALSE;
    ((struct StubEvent *)handle)->signaled = 1;
    return TRUE;
}

BOOL ResetEvent(HANDLE handle)
{
    if (handle == NULL)
        return FALSE;
    ((struct StubEvent *)handle)->signaled = 0;
    return TRUE;
}

DWORD WaitForSingleObject(HANDLE handle, DWORD timeout)
{
    struct StubEvent *event_handle;
    (void)timeout;
    if (handle == NULL)
        return WAIT_FAILED;
    event_handle = (struct StubEvent *)handle;
    if (!event_handle->signaled)
        return WAIT_TIMEOUT;
    if (!event_handle->manual_reset)
        event_handle->signaled = 0;
    return WAIT_OBJECT_0;
}

DWORD GetCurrentProcessId(void)
{
    return 4242UL;
}
