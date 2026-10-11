/* Included by the Win32 backend; also exercised with a deterministic shim. */
#define O2_THREAD_SLOTS 64U
struct O2TrackedThread { HANDLE handle; DWORD tid; };
static struct O2TrackedThread o2_threads[O2_THREAD_SLOTS];
static CRITICAL_SECTION o2_threads_lock;
static LONG o2_threads_state;
static void o2_threads_init(void)
{
    if (InterlockedCompareExchange(&o2_threads_state, 1, 0) == 0) {
        InitializeCriticalSection(&o2_threads_lock);
        InterlockedExchange(&o2_threads_state, 2);
    } else while (InterlockedCompareExchange(&o2_threads_state, 2, 2) != 2)
        Sleep(0);
}
static O2APIRET o2win_DosWaitThread(O2ULONG *ptid, O2ULONG option)
{
    HANDLE copies[O2_THREAD_SLOTS], originals[O2_THREAD_SLOTS];
    DWORD ids[O2_THREAD_SLOTS], result, error;
    unsigned int slots[O2_THREAD_SLOTS], i, n, selected;
    O2APIRET rc;
    if (!ptid || option > 1UL) return 87UL;
    if (*ptid == GetCurrentThreadId()) return 309UL;
    o2_threads_init();
    n = 0; rc = 0;
    EnterCriticalSection(&o2_threads_lock);
    for (i = 0; i < O2_THREAD_SLOTS; ++i) {
        if (!o2_threads[i].handle || o2_threads[i].tid == GetCurrentThreadId()) continue;
        if (*ptid && o2_threads[i].tid != *ptid) continue;
        if (!DuplicateHandle(GetCurrentProcess(), o2_threads[i].handle,
                             GetCurrentProcess(), &copies[n], 0, FALSE,
                             DUPLICATE_SAME_ACCESS)) {
            rc = (O2APIRET)GetLastError(); break;
        }
        originals[n] = o2_threads[i].handle;
        ids[n] = o2_threads[i].tid; slots[n] = i; ++n;
    }
    LeaveCriticalSection(&o2_threads_lock);
    if (!rc && !n) rc = 309UL;
    if (!rc) {
        result = WaitForMultipleObjects(n, copies, FALSE, option ? 0 : INFINITE);
        error = GetLastError();
        if (result == WAIT_TIMEOUT) rc = 294UL;
        else if (result == WAIT_FAILED) rc = (O2APIRET)error;
        else if (result >= WAIT_OBJECT_0 + n) rc = 309UL;
        else {
            selected = (unsigned int)(result - WAIT_OBJECT_0);
            i = slots[selected];
            EnterCriticalSection(&o2_threads_lock);
            if (o2_threads[i].handle != originals[selected] || o2_threads[i].tid != ids[selected])
                rc = 309UL; /* another waiter already reaped this thread */
            else {
                CloseHandle(o2_threads[i].handle);
                o2_threads[i].handle = NULL;
                *ptid = (O2ULONG)ids[selected];
            }
            LeaveCriticalSection(&o2_threads_lock);
        }
    }
    for (i = 0; i < n; ++i) CloseHandle(copies[i]);
    return rc;
}
