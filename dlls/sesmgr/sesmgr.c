/*
 * sesmgr.c - minimal OS/2 2.x Session Manager personality for Win32.
 *
 * M29L1 fleshes out the real SESMGR path used by reconstructed CMD START.
 * DosStartSession (ordinal 17) is deliberately separate from DOSCALLS.283:
 * it creates a new host session and returns immediately rather than creating
 * a normal parent/child process relationship for DosWaitChild.
 *
 * The STARTDATA wire layout is the 32-bit OS/2 layout packed on two-byte
 * boundaries.  Its full form is 60 bytes; the API also historically accepts
 * shorter 24/30/32/50-byte prefixes.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#ifndef __cdecl
#define __cdecl
#endif

#define O2_NO_ERROR                    0UL
#define O2_ERROR_NOT_ENOUGH_MEMORY     8UL
#define O2_ERROR_INVALID_PARAMETER    87UL
#define O2_ERROR_SMG_INVALID_SESSION_ID 369UL
#define O2_ERROR_SMG_INVALID_CALL    418UL
#define O2_ERROR_SMG_INVALID_STOP_OPTION 458UL
#define O2_ERROR_SMG_PROCESS_NOT_PARENT 460UL
#define O2_ERROR_SMG_RETRY_SUB_ALLOC 463UL

#define O2_SSF_RELATED_INDEPENDENT 0U
#define O2_SSF_RELATED_CHILD       1U
#define O2_SSF_FGBG_FORE           0U
#define O2_SSF_FGBG_BACK           1U
#define O2_SSF_TRACEOPT_NONE       0U
#define O2_SSF_INHERTOPT_SHELL     0U
#define O2_SSF_INHERTOPT_PARENT    1U
#define O2_SSF_TYPE_DEFAULT        0U
#define O2_SSF_TYPE_FULLSCREEN     1U
#define O2_SSF_TYPE_WINDOWABLEVIO  2U
#define O2_SSF_TYPE_PM             3U

#define O2_SSF_CONTROL_VISIBLE      0x0000U
#define O2_SSF_CONTROL_INVISIBLE    0x0001U
#define O2_SSF_CONTROL_MAXIMIZE     0x0002U
#define O2_SSF_CONTROL_MINIMIZE     0x0004U
#define O2_SSF_CONTROL_NOAUTOCLOSE  0x0008U
#define O2_SSF_CONTROL_SETPOS       0x8000U
#define O2_SSF_CONTROL_KNOWN_MASK   0x800FU

#define O2_SESSION_MAX 64
#define O2_SESSION_SHARED_MAX 128
#define O2_SESSION_TEXT_MAX 260U
#define O2_SESSION_CMDLINE_MAX 4096U
#define O2_SESSION_REGISTRY_MAGIC 0x4F325352UL
#define O2_SESSION_REGISTRY_VERSION 1UL
#define O2_SESSION_MAP_NAME "Local\\OS2HOST32_SESMGR_R1"
#define O2_SESSION_MUTEX_NAME "Local\\OS2HOST32_SESMGR_R1_MUTEX"

#pragma pack(push, 2)
struct O2StartData {
    unsigned short Length;
    unsigned short Related;
    unsigned short FgBg;
    unsigned short TraceOpt;
    char *PgmTitle;
    char *PgmName;
    unsigned char *PgmInputs;
    unsigned char *TermQ;
    unsigned char *Environment;
    unsigned short InheritOpt;
    unsigned short SessionType;
    char *IconFile;
    unsigned long PgmHandle;
    unsigned short PgmControl;
    unsigned short InitXPos;
    unsigned short InitYPos;
    unsigned short InitXSize;
    unsigned short InitYSize;
    unsigned short Reserved;
    char *ObjectBuffer;
    unsigned long ObjectBuffLen;
};
#pragma pack(pop)

typedef char O2StartData_must_be_60_bytes[
    (sizeof(struct O2StartData) == 60) ? 1 : -1];

struct O2Session {
    unsigned long sessionId;
    DWORD pid;
    HANDLE process;
    HANDLE job;
};

struct O2SessionInfoWire {
    unsigned long taskId;
    unsigned long pid;
    char program[O2_SESSION_TEXT_MAX];
    char title[O2_SESSION_TEXT_MAX];
};

struct O2SharedSession {
    unsigned long inUse;
    unsigned long sessionId;
    DWORD pid;
    DWORD ownerPid;
    DWORD createTimeLow;
    DWORD createTimeHigh;
    DWORD ownerTimeLow;
    DWORD ownerTimeHigh;
    char program[O2_SESSION_TEXT_MAX];
    char title[O2_SESSION_TEXT_MAX];
};

struct O2SessionRegistry {
    unsigned long magic;
    unsigned long version;
    unsigned long nextSessionId;
    struct O2SharedSession sessions[O2_SESSION_SHARED_MAX];
};

static struct O2Session g_sessions[O2_SESSION_MAX];
static CRITICAL_SECTION g_session_lock;
static int g_session_lock_ready;
static HANDLE g_registry_map;
static HANDLE g_registry_mutex;
static struct O2SessionRegistry *g_registry;

static int session_trace_enabled(void)
{
    char b[4];
    return GetEnvironmentVariableA("OS2_TRACE_SESSION", b, sizeof(b)) != 0;
}

static int append_text(char *dst, unsigned cap, const char *text)
{
    size_t have;
    size_t add;

    have = strlen(dst);
    add = strlen(text);
    if (have + add + 1U > (size_t)cap)
        return 0;
    memcpy(dst + have, text, add + 1U);
    return 1;
}

static int append_quoted_arg(char *dst, unsigned cap, const char *arg)
{
    size_t have;
    const char *p;

    if (arg == NULL)
        arg = "";
    have = strlen(dst);
    if (have + 3U > (size_t)cap)
        return 0;
    dst[have++] = '"';
    dst[have] = '\0';
    p = arg;
    while (*p != '\0') {
        if (*p == '"') {
            if (have + 2U >= (size_t)cap)
                return 0;
            dst[have++] = '\\';
            dst[have++] = '"';
        } else {
            if (have + 1U >= (size_t)cap)
                return 0;
            dst[have++] = *p;
        }
        ++p;
    }
    if (have + 2U > (size_t)cap)
        return 0;
    dst[have++] = '"';
    dst[have] = '\0';
    return 1;
}

static int env_entry_valid(const char *s)
{
    const char *eq;

    if (s == NULL || *s == '\0')
        return 0;
    if (s[0] == '=') {
        eq = strchr(s + 1, '=');
        return eq != NULL && eq > s + 1;
    }
    eq = strchr(s, '=');
    return eq != NULL && eq != s;
}

static char *normalize_environment(const char *env, SIZE_T *bytesOut,
                                   unsigned long *errorOut)
{
    const char *p;
    SIZE_T bytes;
    SIZE_T len;
    SIZE_T count;
    SIZE_T i;
    SIZE_T j;
    char **items;
    char *out;
    char *dst;
    char *tmp;

    if (bytesOut != NULL)
        *bytesOut = 0U;
    if (errorOut != NULL)
        *errorOut = O2_NO_ERROR;
    if (env == NULL)
        return NULL;

    p = env;
    bytes = 0U;
    count = 0U;
    for (;;) {
        len = strlen(p);
        if (len == 0U) {
            ++bytes;
            break;
        }
        if (!env_entry_valid(p)) {
            if (errorOut != NULL)
                *errorOut = O2_ERROR_INVALID_PARAMETER;
            return NULL;
        }
        if (bytes + len + 1U >= 32767U) {
            if (errorOut != NULL)
                *errorOut = O2_ERROR_INVALID_PARAMETER;
            return NULL;
        }
        bytes += len + 1U;
        ++count;
        p += len + 1U;
    }

    if (count == 0U) {
        out = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 2U);
        if (out == NULL) {
            if (errorOut != NULL)
                *errorOut = O2_ERROR_NOT_ENOUGH_MEMORY;
            return NULL;
        }
        if (bytesOut != NULL)
            *bytesOut = 2U;
        return out;
    }

    items = (char **)HeapAlloc(GetProcessHeap(), 0,
                               count * sizeof(items[0]));
    if (items == NULL) {
        if (errorOut != NULL)
            *errorOut = O2_ERROR_NOT_ENOUGH_MEMORY;
        return NULL;
    }
    p = env;
    for (i = 0U; i < count; ++i) {
        items[i] = (char *)p;
        p += strlen(p) + 1U;
    }
    for (i = 1U; i < count; ++i) {
        tmp = items[i];
        j = i;
        while (j != 0U && lstrcmpiA(items[j - 1U], tmp) > 0) {
            items[j] = items[j - 1U];
            --j;
        }
        items[j] = tmp;
    }

    out = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes);
    if (out == NULL) {
        HeapFree(GetProcessHeap(), 0, items);
        if (errorOut != NULL)
            *errorOut = O2_ERROR_NOT_ENOUGH_MEMORY;
        return NULL;
    }
    dst = out;
    for (i = 0U; i < count; ++i) {
        len = strlen(items[i]) + 1U;
        memcpy(dst, items[i], len);
        dst += len;
    }
    *dst++ = '\0';
    HeapFree(GetProcessHeap(), 0, items);
    if (bytesOut != NULL)
        *bytesOut = (SIZE_T)(dst - out);
    return out;
}

static void set_object_buffer(const struct O2StartData *sd,
                              const char *text)
{
    size_t n;

    if (sd == NULL || sd->Length < 60U || sd->ObjectBuffer == NULL ||
        sd->ObjectBuffLen == 0UL)
        return;
    if (text == NULL)
        text = "";
    n = strlen(text);
    if (n >= (size_t)sd->ObjectBuffLen)
        n = (size_t)sd->ObjectBuffLen - 1U;
    memcpy(sd->ObjectBuffer, text, n);
    sd->ObjectBuffer[n] = '\0';
}

static void copy_session_text(char *dst, unsigned cap, const char *src)
{
    size_t n;

    if (cap == 0U)
        return;
    if (src == NULL)
        src = "";
    n = strlen(src);
    if (n >= (size_t)cap)
        n = (size_t)cap - 1U;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static int process_creation_time(HANDLE process, DWORD *low, DWORD *high)
{
    FILETIME createTime;
    FILETIME exitTime;
    FILETIME kernelTime;
    FILETIME userTime;

    if (low != NULL)
        *low = 0U;
    if (high != NULL)
        *high = 0U;
    /* GetCurrentProcess() is the valid Win32 pseudo-handle (HANDLE)-1,
     * numerically identical to INVALID_HANDLE_VALUE.  Do not reject -1 here:
     * GetProcessTimes explicitly accepts the current-process pseudo-handle.
     * A genuinely bad non-NULL handle will simply make GetProcessTimes fail. */
    if (process == NULL)
        return 0;
    if (!GetProcessTimes(process, &createTime, &exitTime,
                         &kernelTime, &userTime))
        return 0;
    if (low != NULL)
        *low = createTime.dwLowDateTime;
    if (high != NULL)
        *high = createTime.dwHighDateTime;
    return 1;
}

static int ensure_registry(void)
{
    DWORD waitRc;

    if (g_registry != NULL && g_registry_mutex != NULL)
        return 1;

    if (g_registry_mutex == NULL) {
        g_registry_mutex = CreateMutexA(NULL, FALSE, O2_SESSION_MUTEX_NAME);
        if (g_registry_mutex == NULL)
            return 0;
    }
    if (g_registry_map == NULL) {
        g_registry_map = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
                                             PAGE_READWRITE, 0,
                                             (DWORD)sizeof(*g_registry),
                                             O2_SESSION_MAP_NAME);
        if (g_registry_map == NULL)
            return 0;
    }
    if (g_registry == NULL) {
        g_registry = (struct O2SessionRegistry *)MapViewOfFile(
            g_registry_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*g_registry));
        if (g_registry == NULL)
            return 0;
    }

    waitRc = WaitForSingleObject(g_registry_mutex, INFINITE);
    if (waitRc != WAIT_OBJECT_0 && waitRc != WAIT_ABANDONED)
        return 0;
    if (g_registry->magic != O2_SESSION_REGISTRY_MAGIC ||
        g_registry->version != O2_SESSION_REGISTRY_VERSION) {
        memset(g_registry, 0, sizeof(*g_registry));
        g_registry->magic = O2_SESSION_REGISTRY_MAGIC;
        g_registry->version = O2_SESSION_REGISTRY_VERSION;
    }
    ReleaseMutex(g_registry_mutex);
    return 1;
}

static int registry_lock(void)
{
    DWORD waitRc;

    if (!ensure_registry())
        return 0;
    waitRc = WaitForSingleObject(g_registry_mutex, INFINITE);
    return waitRc == WAIT_OBJECT_0 || waitRc == WAIT_ABANDONED;
}

static void registry_unlock(void)
{
    if (g_registry_mutex != NULL)
        ReleaseMutex(g_registry_mutex);
}

static int process_matches_times(DWORD pid, DWORD low, DWORD high)
{
    HANDLE process;
    DWORD code;
    DWORD actualLow;
    DWORD actualHigh;
    int same;

    process = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
                          FALSE, pid);
    if (process == NULL)
        return GetLastError() == ERROR_INVALID_PARAMETER ? 0 : 1;
    code = STILL_ACTIVE;
    if (GetExitCodeProcess(process, &code) && code != STILL_ACTIVE) {
        CloseHandle(process);
        return 0;
    }
    actualLow = 0U;
    actualHigh = 0U;
    same = process_creation_time(process, &actualLow, &actualHigh);
    CloseHandle(process);
    if (!same)
        return 1;
    return actualLow == low && actualHigh == high;
}

static void registry_reap_locked(void)
{
    unsigned i;

    for (i = 0U; i < O2_SESSION_SHARED_MAX; ++i) {
        struct O2SharedSession *e;
        e = &g_registry->sessions[i];
        if (!e->inUse)
            continue;
        if (!process_matches_times(e->pid, e->createTimeLow,
                                   e->createTimeHigh) ||
            !process_matches_times(e->ownerPid, e->ownerTimeLow,
                                   e->ownerTimeHigh))
            memset(e, 0, sizeof(*e));
    }
}

static unsigned long registry_allocate_session_id(void)
{
    unsigned long sid;

    if (!registry_lock())
        return 0UL;
    registry_reap_locked();
    sid = ++g_registry->nextSessionId;
    if (sid == 0UL)
        sid = ++g_registry->nextSessionId;
    registry_unlock();
    return sid;
}

static int registry_add_session(unsigned long sid, DWORD pid,
                                HANDLE process, const char *program,
                                const char *title)
{
    DWORD childLow;
    DWORD childHigh;
    DWORD ownerLow;
    DWORD ownerHigh;
    DWORD ownerPid;
    unsigned i;

    childLow = 0U;
    childHigh = 0U;
    ownerLow = 0U;
    ownerHigh = 0U;
    if (!process_creation_time(process, &childLow, &childHigh) ||
        !process_creation_time(GetCurrentProcess(), &ownerLow, &ownerHigh))
        return 0;
    ownerPid = GetCurrentProcessId();

    if (!registry_lock())
        return 0;
    registry_reap_locked();
    for (i = 0U; i < O2_SESSION_SHARED_MAX; ++i) {
        struct O2SharedSession *e;
        e = &g_registry->sessions[i];
        if (e->inUse)
            continue;
        memset(e, 0, sizeof(*e));
        e->inUse = 1UL;
        e->sessionId = sid;
        e->pid = pid;
        e->ownerPid = ownerPid;
        e->createTimeLow = childLow;
        e->createTimeHigh = childHigh;
        e->ownerTimeLow = ownerLow;
        e->ownerTimeHigh = ownerHigh;
        copy_session_text(e->program, O2_SESSION_TEXT_MAX, program);
        copy_session_text(e->title, O2_SESSION_TEXT_MAX,
                          title != NULL && *title != '\0' ? title : program);
        registry_unlock();
        return 1;
    }
    registry_unlock();
    return 0;
}

static void registry_remove_session(unsigned long sid)
{
    unsigned i;

    if (!registry_lock())
        return;
    for (i = 0U; i < O2_SESSION_SHARED_MAX; ++i) {
        if (g_registry->sessions[i].inUse &&
            g_registry->sessions[i].sessionId == sid) {
            memset(&g_registry->sessions[i], 0,
                   sizeof(g_registry->sessions[i]));
            break;
        }
    }
    registry_unlock();
}

static unsigned long registry_set_title(unsigned long sid, const char *title)
{
    DWORD currentPid;
    unsigned i;

    if (title == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    currentPid = GetCurrentProcessId();
    if (!registry_lock())
        return O2_ERROR_NOT_ENOUGH_MEMORY;
    registry_reap_locked();
    for (i = 0U; i < O2_SESSION_SHARED_MAX; ++i) {
        struct O2SharedSession *e;
        e = &g_registry->sessions[i];
        if (!e->inUse)
            continue;
        if ((sid != 0UL && e->sessionId != sid) ||
            (sid == 0UL && e->pid != currentPid))
            continue;
        if (e->pid != currentPid && e->ownerPid != currentPid) {
            registry_unlock();
            return O2_ERROR_SMG_PROCESS_NOT_PARENT;
        }
        {
            int isCurrent;
            isCurrent = e->pid == currentPid;
            copy_session_text(e->title, O2_SESSION_TEXT_MAX, title);
            registry_unlock();
            if (isCurrent)
                SetConsoleTitleA(title);
        }
        return O2_NO_ERROR;
    }
    registry_unlock();
    return O2_ERROR_SMG_INVALID_SESSION_ID;
}

static unsigned long registry_query_owned(struct O2SessionInfoWire *entries,
                                          unsigned long capacity,
                                          unsigned long *count)
{
    DWORD ownerPid;
    DWORD ownerLow;
    DWORD ownerHigh;
    unsigned long n;
    unsigned i;

    if (count == NULL || (capacity != 0UL && entries == NULL))
        return O2_ERROR_INVALID_PARAMETER;
    *count = 0UL;
    ownerPid = GetCurrentProcessId();
    ownerLow = 0U;
    ownerHigh = 0U;
    if (!process_creation_time(GetCurrentProcess(), &ownerLow, &ownerHigh))
        return O2_ERROR_INVALID_PARAMETER;
    if (!registry_lock())
        return O2_ERROR_NOT_ENOUGH_MEMORY;
    registry_reap_locked();
    n = 0UL;
    for (i = 0U; i < O2_SESSION_SHARED_MAX; ++i) {
        struct O2SharedSession *e;
        e = &g_registry->sessions[i];
        if (!e->inUse || e->ownerPid != ownerPid ||
            e->ownerTimeLow != ownerLow || e->ownerTimeHigh != ownerHigh)
            continue;
        if (n < capacity) {
            entries[n].taskId = e->sessionId;
            entries[n].pid = (unsigned long)e->pid;
            copy_session_text(entries[n].program, O2_SESSION_TEXT_MAX,
                              e->program);
            copy_session_text(entries[n].title, O2_SESSION_TEXT_MAX,
                              e->title);
        }
        ++n;
    }
    registry_unlock();
    *count = n < capacity ? n : capacity;
    return O2_NO_ERROR;
}

/* SESMGR.5 -- the historical Session Manager title update.  V1 exposes a
 * flat 32-bit personality for flat guests; mixed 16-bit callers still need
 * their normal thunk path.  A zero session id is accepted as a V1 extension
 * meaning the current hosted process. */
unsigned long __cdecl DosSmSetTitle(unsigned long sessionId, char *title)
{
    return registry_set_title(sessionId, title);
}

/* Private V1 query used by reconstructed CMD32 PS.  This is intentionally
 * outside the historical SESMGR ordinal range. */
unsigned long __cdecl O2HostQuerySessions(struct O2SessionInfoWire *entries,
                                          unsigned long capacity,
                                          unsigned long *count)
{
    return registry_query_owned(entries, capacity, count);
}

static void reap_sessions_locked(void)
{
    int i;
    DWORD code;

    for (i = 0; i < O2_SESSION_MAX; ++i) {
        if (g_sessions[i].process == NULL)
            continue;
        code = STILL_ACTIVE;
        if (GetExitCodeProcess(g_sessions[i].process, &code) &&
            code != STILL_ACTIVE) {
            registry_remove_session(g_sessions[i].sessionId);
            CloseHandle(g_sessions[i].process);
            if (g_sessions[i].job != NULL)
                CloseHandle(g_sessions[i].job);
            memset(&g_sessions[i], 0, sizeof(g_sessions[i]));
        }
    }
}

static int remember_session(unsigned long sid, DWORD pid, HANDLE process,
                            HANDLE job, const char *program,
                            const char *title)
{
    int i;

    if (!g_session_lock_ready)
        return 0;
    EnterCriticalSection(&g_session_lock);
    reap_sessions_locked();
    for (i = 0; i < O2_SESSION_MAX; ++i) {
        if (g_sessions[i].process == NULL) {
            g_sessions[i].sessionId = sid;
            g_sessions[i].pid = pid;
            g_sessions[i].process = process;
            g_sessions[i].job = job;
            if (!registry_add_session(sid, pid, process, program, title)) {
                memset(&g_sessions[i], 0, sizeof(g_sessions[i]));
                LeaveCriticalSection(&g_session_lock);
                return 0;
            }
            LeaveCriticalSection(&g_session_lock);
            return 1;
        }
    }
    LeaveCriticalSection(&g_session_lock);
    return 0;
}

/* SESMGR.8 -- stop one or all related child sessions owned by this process.
 * The V1 host uses forced host-process termination: unlike historical OS/2
 * there is not yet a cooperative session-close protocol to refuse/confirm. */
unsigned long __cdecl DosStopSession(unsigned long scope,
                                     unsigned long sessionId)
{
    int i;
    int found;

    if (scope > 1UL)
        return O2_ERROR_SMG_INVALID_STOP_OPTION;
    if (scope == 0UL && sessionId == 0UL)
        return O2_ERROR_SMG_INVALID_SESSION_ID;
    if (!g_session_lock_ready)
        return O2_ERROR_SMG_INVALID_CALL;

    found = 0;
    EnterCriticalSection(&g_session_lock);
    reap_sessions_locked();
    for (i = 0; i < O2_SESSION_MAX; ++i) {
        HANDLE process;
        if (g_sessions[i].process == NULL)
            continue;
        if (scope == 0UL && g_sessions[i].sessionId != sessionId)
            continue;
        process = g_sessions[i].process;
        found = 1;
        if (session_trace_enabled()) {
            fprintf(stderr, "SESMGR STOP: session=%lu pid=%lu\n",
                    g_sessions[i].sessionId,
                    (unsigned long)g_sessions[i].pid);
            fflush(stderr);
        }
        {
            BOOL stopped;
            stopped = FALSE;
            if (g_sessions[i].job != NULL)
                stopped = TerminateJobObject(g_sessions[i].job, 1U);
            if (!stopped)
                stopped = TerminateProcess(process, 1U);
            if (!stopped) {
                unsigned long stopError;
                stopError = (unsigned long)GetLastError();
                LeaveCriticalSection(&g_session_lock);
                return stopError != 0UL ? stopError : O2_ERROR_SMG_INVALID_CALL;
            }
        }
        (void)WaitForSingleObject(process, 5000U);
        registry_remove_session(g_sessions[i].sessionId);
        CloseHandle(process);
        if (g_sessions[i].job != NULL)
            CloseHandle(g_sessions[i].job);
        memset(&g_sessions[i], 0, sizeof(g_sessions[i]));
        if (scope == 0UL)
            break;
    }
    LeaveCriticalSection(&g_session_lock);

    if (!found)
        return O2_ERROR_SMG_INVALID_SESSION_ID;
    return O2_NO_ERROR;
}

/* SESMGR.17 */
unsigned long __cdecl DosStartSession(struct O2StartData *sd,
                                      unsigned long *sessionId,
                                      unsigned long *pidOut)
{
    char host[MAX_PATH];
    char *command;
    const char *inputs;
    const char *env;
    DWORD hostLen;
    DWORD creationFlags;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    HANDLE sessionJob;
    BOOL ok;
    char *normalizedEnv;
    SIZE_T normalizedBytes;
    unsigned long envRc;
    unsigned long sid;
    unsigned short inheritOpt;
    unsigned short sessionType;
    unsigned short pgmControl;
    unsigned short initXPos;
    unsigned short initYPos;
    unsigned short initXSize;
    unsigned short initYSize;
    const char *iconFile;
    unsigned long pgmHandle;

    if (sessionId != NULL)
        *sessionId = 0UL;
    if (pidOut != NULL)
        *pidOut = 0UL;
    if (sd == NULL || sd->Length < 24U || sd->PgmName == NULL ||
        sd->PgmName[0] == '\0')
        return O2_ERROR_INVALID_PARAMETER;
    set_object_buffer(sd, "");
    if (sd->Related != O2_SSF_RELATED_INDEPENDENT &&
        sd->Related != O2_SSF_RELATED_CHILD)
        return O2_ERROR_INVALID_PARAMETER;
    if (sd->FgBg != O2_SSF_FGBG_FORE && sd->FgBg != O2_SSF_FGBG_BACK)
        return O2_ERROR_INVALID_PARAMETER;
    if (sd->TraceOpt != O2_SSF_TRACEOPT_NONE)
        return O2_ERROR_SMG_INVALID_CALL;

    inheritOpt = O2_SSF_INHERTOPT_SHELL;
    if (sd->Length >= 30U)
        inheritOpt = sd->InheritOpt;
    if (inheritOpt != O2_SSF_INHERTOPT_SHELL &&
        inheritOpt != O2_SSF_INHERTOPT_PARENT)
        return O2_ERROR_INVALID_PARAMETER;

    sessionType = O2_SSF_TYPE_DEFAULT;
    if (sd->Length >= 32U)
        sessionType = sd->SessionType;
    if (sessionType != O2_SSF_TYPE_DEFAULT &&
        sessionType != O2_SSF_TYPE_FULLSCREEN &&
        sessionType != O2_SSF_TYPE_WINDOWABLEVIO &&
        sessionType != O2_SSF_TYPE_PM)
        return O2_ERROR_SMG_INVALID_CALL;

    iconFile = NULL;
    pgmHandle = 0UL;
    pgmControl = O2_SSF_CONTROL_VISIBLE;
    initXPos = 0U;
    initYPos = 0U;
    initXSize = 0U;
    initYSize = 0U;
    if (sd->Length >= 36U)
        iconFile = sd->IconFile;
    if (sd->Length >= 40U)
        pgmHandle = sd->PgmHandle;
    if (sd->Length >= 42U)
        pgmControl = sd->PgmControl;
    if ((pgmControl & ~O2_SSF_CONTROL_KNOWN_MASK) != 0U ||
        ((pgmControl & O2_SSF_CONTROL_MAXIMIZE) != 0U &&
         (pgmControl & O2_SSF_CONTROL_MINIMIZE) != 0U))
        return O2_ERROR_INVALID_PARAMETER;
    if (sd->Length >= 50U) {
        initXPos = sd->InitXPos;
        initYPos = sd->InitYPos;
        initXSize = sd->InitXSize;
        initYSize = sd->InitYSize;
    }
    if (sd->Length >= 52U && sd->Reserved != 0U)
        return O2_ERROR_INVALID_PARAMETER;

    /* Termination queues and install-database program handles are fields of
     * STARTDATA, but their backing OS/2 services do not exist yet.  Reject a
     * non-empty request rather than silently pretending those semantics. */
    if (sd->Length >= 24U && sd->TermQ != NULL && sd->TermQ[0] != '\0')
        return O2_ERROR_SMG_INVALID_CALL;
    if (pgmHandle != 0UL)
        return O2_ERROR_SMG_INVALID_CALL;

    hostLen = GetEnvironmentVariableA("OS2HOST32_LOADER", host,
                                      (DWORD)sizeof(host));
    if (hostLen == 0U) {
        hostLen = GetModuleFileNameA(NULL, host, (DWORD)sizeof(host));
        if (hostLen == 0U || hostLen >= (DWORD)sizeof(host)) {
            set_object_buffer(sd, "os2host32.exe");
            return (unsigned long)GetLastError();
        }
    } else if (hostLen >= (DWORD)sizeof(host)) {
        set_object_buffer(sd, "os2host32.exe");
        return O2_ERROR_INVALID_PARAMETER;
    }

    command = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                O2_SESSION_CMDLINE_MAX);
    if (command == NULL)
        return O2_ERROR_NOT_ENOUGH_MEMORY;
    if (!append_quoted_arg(command, O2_SESSION_CMDLINE_MAX, host) ||
        !append_text(command, O2_SESSION_CMDLINE_MAX, " --run-quiet ") ||
        !append_quoted_arg(command, O2_SESSION_CMDLINE_MAX, sd->PgmName)) {
        HeapFree(GetProcessHeap(), 0, command);
        return O2_ERROR_INVALID_PARAMETER;
    }
    inputs = (const char *)sd->PgmInputs;
    if (inputs != NULL && *inputs != '\0') {
        if (!append_text(command, O2_SESSION_CMDLINE_MAX, " ") ||
            !append_text(command, O2_SESSION_CMDLINE_MAX, inputs)) {
            HeapFree(GetProcessHeap(), 0, command);
            return O2_ERROR_INVALID_PARAMETER;
        }
    }

    env = NULL;
    if (sd->Length >= 28U)
        env = (const char *)sd->Environment;
    normalizedEnv = NULL;
    normalizedBytes = 0U;
    envRc = O2_NO_ERROR;
    if (env != NULL) {
        normalizedEnv = normalize_environment(env, &normalizedBytes, &envRc);
        if (normalizedEnv == NULL) {
            HeapFree(GetProcessHeap(), 0, command);
            return envRc != O2_NO_ERROR ? envRc : O2_ERROR_INVALID_PARAMETER;
        }
    }

    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    si.lpTitle = sd->PgmTitle;

    /* STARTDATA.PgmControl applies to windowed sessions.  Windows cannot
     * recreate OS/2 full-screen VIO, but STARTUPINFO gives us faithful
     * visible/invisible/min/max and initial geometry semantics for a new
     * console host. */
    if (sessionType != O2_SSF_TYPE_FULLSCREEN) {
        if ((pgmControl & O2_SSF_CONTROL_INVISIBLE) != 0U) {
            si.dwFlags |= STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
        } else if ((pgmControl & O2_SSF_CONTROL_MAXIMIZE) != 0U) {
            si.dwFlags |= STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_SHOWMAXIMIZED;
        } else if ((pgmControl & O2_SSF_CONTROL_MINIMIZE) != 0U) {
            si.dwFlags |= STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_SHOWMINIMIZED;
        }
        if ((pgmControl & O2_SSF_CONTROL_SETPOS) != 0U) {
            si.dwFlags |= STARTF_USEPOSITION | STARTF_USESIZE;
            si.dwX = (DWORD)initXPos;
            si.dwY = (DWORD)initYPos;
            si.dwXSize = (DWORD)initXSize;
            si.dwYSize = (DWORD)initYSize;
        }
    }

    creationFlags = CREATE_NEW_PROCESS_GROUP;
    if (sd->Related == O2_SSF_RELATED_CHILD)
        creationFlags |= CREATE_SUSPENDED;
    if (sessionType == O2_SSF_TYPE_PM) {
        /* PM sessions do not require a VIO console.  The hosted PMWIN
         * personality supplies the GUI surface. */
        creationFlags |= DETACHED_PROCESS;
    } else {
        creationFlags |= CREATE_NEW_CONSOLE;
        if (sd->FgBg == O2_SSF_FGBG_BACK &&
            (si.dwFlags & STARTF_USESHOWWINDOW) == 0U) {
            si.dwFlags |= STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_SHOWNOACTIVATE;
        }
    }

    if (session_trace_enabled()) {
        fprintf(stderr,
                "SESMGR START: program=[%s] inputs=[%s] title=[%s] related=%u fgbg=%u type=%u inherit=%u control=0x%04X pos=%u,%u size=%u,%u create=0x%08lX env=%s bytes=%lu\n",
                sd->PgmName,
                inputs != NULL ? inputs : "",
                sd->PgmTitle != NULL ? sd->PgmTitle : "",
                (unsigned)sd->Related, (unsigned)sd->FgBg,
                (unsigned)sessionType, (unsigned)inheritOpt,
                (unsigned)pgmControl,
                (unsigned)initXPos, (unsigned)initYPos,
                (unsigned)initXSize, (unsigned)initYSize,
                (unsigned long)creationFlags,
                normalizedEnv != NULL ? "custom" : "inherit",
                (unsigned long)normalizedBytes);
        if (iconFile != NULL && *iconFile != '\0')
            fprintf(stderr, "SESMGR START: icon=[%s] (recorded; host icon binding not implemented)\n", iconFile);
        fprintf(stderr, "SESMGR START: command=[%s]\n", command);
        fflush(stderr);
    }

    sessionJob = NULL;
    sid = registry_allocate_session_id();
    if (sid == 0UL) {
        if (normalizedEnv != NULL)
            HeapFree(GetProcessHeap(), 0, normalizedEnv);
        HeapFree(GetProcessHeap(), 0, command);
        return O2_ERROR_NOT_ENOUGH_MEMORY;
    }

    ok = CreateProcessA(NULL, command, NULL, NULL, FALSE, creationFlags,
                        normalizedEnv, NULL, &si, &pi);
    if (normalizedEnv != NULL)
        HeapFree(GetProcessHeap(), 0, normalizedEnv);
    HeapFree(GetProcessHeap(), 0, command);
    if (!ok) {
        set_object_buffer(sd, sd->PgmName);
        return (unsigned long)GetLastError();
    }

    if (sd->Related == O2_SSF_RELATED_CHILD) {
        sessionJob = CreateJobObjectA(NULL, NULL);
        if (sessionJob != NULL &&
            !AssignProcessToJobObject(sessionJob, pi.hProcess)) {
            CloseHandle(sessionJob);
            sessionJob = NULL;
        }
    }

    if (sessionId != NULL)
        *sessionId = sid;
    if (pidOut != NULL)
        *pidOut = (unsigned long)pi.dwProcessId;

    if (sd->Related == O2_SSF_RELATED_CHILD) {
        DWORD resumeRc;
        if (!remember_session(sid, pi.dwProcessId, pi.hProcess,
                              sessionJob, sd->PgmName, sd->PgmTitle)) {
            (void)TerminateProcess(pi.hProcess, 1U);
            (void)WaitForSingleObject(pi.hProcess, 5000U);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            if (sessionJob != NULL)
                CloseHandle(sessionJob);
            if (sessionId != NULL)
                *sessionId = 0UL;
            if (pidOut != NULL)
                *pidOut = 0UL;
            return O2_ERROR_SMG_RETRY_SUB_ALLOC;
        }
        resumeRc = ResumeThread(pi.hThread);
        CloseHandle(pi.hThread);
        if (resumeRc == (DWORD)-1) {
            unsigned long resumeError;
            resumeError = (unsigned long)GetLastError();
            (void)DosStopSession(0UL, sid);
            if (sessionId != NULL)
                *sessionId = 0UL;
            if (pidOut != NULL)
                *pidOut = 0UL;
            return resumeError;
        }
    } else {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

    if (session_trace_enabled()) {
        fprintf(stderr, "SESMGR START: session=%lu pid=%lu\n",
                (unsigned long)sid, (unsigned long)pi.dwProcessId);
        fflush(stderr);
    }
    return O2_NO_ERROR;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    int i;
    (void)hinst;
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH) {
        InitializeCriticalSection(&g_session_lock);
        g_session_lock_ready = 1;
        memset(g_sessions, 0, sizeof(g_sessions));
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_session_lock_ready) {
            for (i = 0; i < O2_SESSION_MAX; ++i) {
                if (g_sessions[i].process != NULL)
                    CloseHandle(g_sessions[i].process);
                if (g_sessions[i].job != NULL)
                    CloseHandle(g_sessions[i].job);
            }
            DeleteCriticalSection(&g_session_lock);
            g_session_lock_ready = 0;
        }
        if (g_registry != NULL) {
            UnmapViewOfFile(g_registry);
            g_registry = NULL;
        }
        if (g_registry_map != NULL) {
            CloseHandle(g_registry_map);
            g_registry_map = NULL;
        }
        if (g_registry_mutex != NULL) {
            CloseHandle(g_registry_mutex);
            g_registry_mutex = NULL;
        }
    }
    return TRUE;
}
