/* Win32 execution/storage backend for backend-neutral OS/2 SESMGR. */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "os2_sesmgr.h"
#include "os2_sesmgr_backend.h"
#include "os2_sesmgr_win32.h"

#define O2_SESSION_MAP_NAME "Local\\OS2HOST32_SESMGR_R1"
#define O2_SESSION_MUTEX_NAME "Local\\OS2HOST32_SESMGR_R1_MUTEX"
#define O2_SESSION_CMDLINE_MAX 4096U

struct Os2SesmgrWin32Context {
    CRITICAL_SECTION local_lock;
    int local_lock_ready;
    HANDLE registry_map;
    HANDLE registry_mutex;
    struct Os2SesmgrRegistry *registry;
};

static int trace_enabled(void)
{
    char b[4];
    return GetEnvironmentVariableA("OS2_TRACE_SESSION", b, sizeof(b)) != 0;
}

static int append_text(char *dst, unsigned int cap, const char *text)
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

static int append_quoted_arg(char *dst, unsigned int cap, const char *arg)
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

static char *normalize_environment(const char *env, SIZE_T *bytes_out,
                                   Os2SesmgrApiRet *error_out)
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

    if (bytes_out != NULL)
        *bytes_out = 0U;
    if (error_out != NULL)
        *error_out = OS2_SESMGR_NO_ERROR;
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
            if (error_out != NULL)
                *error_out = OS2_SESMGR_ERROR_INVALID_PARAMETER;
            return NULL;
        }
        if (bytes + len + 1U >= 32767U) {
            if (error_out != NULL)
                *error_out = OS2_SESMGR_ERROR_INVALID_PARAMETER;
            return NULL;
        }
        bytes += len + 1U;
        ++count;
        p += len + 1U;
    }

    if (count == 0U) {
        out = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 2U);
        if (out == NULL) {
            if (error_out != NULL)
                *error_out = OS2_SESMGR_ERROR_NOT_ENOUGH_MEMORY;
            return NULL;
        }
        if (bytes_out != NULL)
            *bytes_out = 2U;
        return out;
    }

    items = (char **)HeapAlloc(GetProcessHeap(), 0,
                               count * sizeof(items[0]));
    if (items == NULL) {
        if (error_out != NULL)
            *error_out = OS2_SESMGR_ERROR_NOT_ENOUGH_MEMORY;
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
        if (error_out != NULL)
            *error_out = OS2_SESMGR_ERROR_NOT_ENOUGH_MEMORY;
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
    if (bytes_out != NULL)
        *bytes_out = (SIZE_T)(dst - out);
    return out;
}

static int identity_from_handle(HANDLE process, DWORD pid,
                                struct Os2SesmgrIdentity *identity)
{
    FILETIME create_time;
    FILETIME exit_time;
    FILETIME kernel_time;
    FILETIME user_time;
    if (identity == NULL || process == NULL)
        return 0;
    memset(identity, 0, sizeof(*identity));
    if (!GetProcessTimes(process, &create_time, &exit_time,
                         &kernel_time, &user_time))
        return 0;
    identity->pid = (Os2SesmgrU32)pid;
    identity->token_low = (Os2SesmgrU32)create_time.dwLowDateTime;
    identity->token_high = (Os2SesmgrU32)create_time.dwHighDateTime;
    return 1;
}

static void win32_local_lock(void *opaque)
{
    struct Os2SesmgrWin32Context *ctx;
    ctx = (struct Os2SesmgrWin32Context *)opaque;
    EnterCriticalSection(&ctx->local_lock);
}

static void win32_local_unlock(void *opaque)
{
    struct Os2SesmgrWin32Context *ctx;
    ctx = (struct Os2SesmgrWin32Context *)opaque;
    LeaveCriticalSection(&ctx->local_lock);
}

static int ensure_registry(struct Os2SesmgrWin32Context *ctx)
{
    if (ctx->registry != NULL && ctx->registry_mutex != NULL)
        return 1;
    if (ctx->registry_mutex == NULL) {
        ctx->registry_mutex = CreateMutexA(NULL, FALSE,
                                           O2_SESSION_MUTEX_NAME);
        if (ctx->registry_mutex == NULL)
            return 0;
    }
    if (ctx->registry_map == NULL) {
        ctx->registry_map = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
                                                PAGE_READWRITE, 0,
                                                (DWORD)sizeof(*ctx->registry),
                                                O2_SESSION_MAP_NAME);
        if (ctx->registry_map == NULL)
            return 0;
    }
    if (ctx->registry == NULL) {
        ctx->registry = (struct Os2SesmgrRegistry *)MapViewOfFile(
            ctx->registry_map, FILE_MAP_ALL_ACCESS, 0, 0,
            sizeof(*ctx->registry));
        if (ctx->registry == NULL)
            return 0;
    }
    return 1;
}

static int win32_registry_lock(void *opaque,
                               struct Os2SesmgrRegistry **registry)
{
    struct Os2SesmgrWin32Context *ctx;
    DWORD wait_rc;
    ctx = (struct Os2SesmgrWin32Context *)opaque;
    if (registry != NULL)
        *registry = NULL;
    if (!ensure_registry(ctx))
        return 0;
    wait_rc = WaitForSingleObject(ctx->registry_mutex, INFINITE);
    if (wait_rc != WAIT_OBJECT_0 && wait_rc != WAIT_ABANDONED)
        return 0;
    if (registry != NULL)
        *registry = ctx->registry;
    return 1;
}

static void win32_registry_unlock(void *opaque)
{
    struct Os2SesmgrWin32Context *ctx;
    ctx = (struct Os2SesmgrWin32Context *)opaque;
    if (ctx->registry_mutex != NULL)
        ReleaseMutex(ctx->registry_mutex);
}

static int win32_current_identity(void *opaque,
                                  struct Os2SesmgrIdentity *identity)
{
    (void)opaque;
    return identity_from_handle(GetCurrentProcess(), GetCurrentProcessId(),
                                identity);
}

static int win32_identity_from_process(void *opaque, Os2SesmgrNative native,
                                       Os2SesmgrU32 pid,
                                       struct Os2SesmgrIdentity *identity)
{
    HANDLE process;
    (void)opaque;
    process = (HANDLE)(ULONG_PTR)native;
    if (process == NULL || pid == 0UL)
        return 0;
    return identity_from_handle(process, (DWORD)pid, identity);
}

static int win32_identity_alive(void *opaque,
                                const struct Os2SesmgrIdentity *identity)
{
    HANDLE process;
    DWORD code;
    struct Os2SesmgrIdentity actual;
    int same;
    (void)opaque;
    if (identity == NULL || identity->pid == 0UL)
        return 0;
    process = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
                          FALSE, (DWORD)identity->pid);
    if (process == NULL)
        return GetLastError() == ERROR_INVALID_PARAMETER ? 0 : 1;
    code = STILL_ACTIVE;
    if (GetExitCodeProcess(process, &code) && code != STILL_ACTIVE) {
        CloseHandle(process);
        return 0;
    }
    memset(&actual, 0, sizeof(actual));
    same = identity_from_handle(process, (DWORD)identity->pid, &actual);
    CloseHandle(process);
    if (!same)
        return 1;
    return actual.token_low == identity->token_low &&
           actual.token_high == identity->token_high;
}

static int win32_owned_process_alive(void *opaque, Os2SesmgrNative native)
{
    HANDLE process;
    DWORD code;
    (void)opaque;
    process = (HANDLE)(ULONG_PTR)native;
    if (process == NULL)
        return 0;
    code = STILL_ACTIVE;
    if (GetExitCodeProcess(process, &code) && code != STILL_ACTIVE)
        return 0;
    return 1;
}

static Os2SesmgrApiRet win32_launch(void *opaque,
                                    struct Os2SesmgrStartRequest *request,
                                    struct Os2SesmgrLaunch *launch)
{
    char host[MAX_PATH];
    char *command;
    DWORD host_len;
    DWORD creation_flags;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    HANDLE job;
    BOOL ok;
    const char *inputs;
    char *normalized_env;
    SIZE_T normalized_bytes;
    Os2SesmgrApiRet env_rc;
    (void)opaque;

    memset(launch, 0, sizeof(*launch));
    host_len = GetEnvironmentVariableA("OS2HOST32_LOADER", host,
                                       (DWORD)sizeof(host));
    if (host_len == 0U) {
        host_len = GetModuleFileNameA(NULL, host, (DWORD)sizeof(host));
        if (host_len == 0U || host_len >= (DWORD)sizeof(host)) {
            os2_sesmgr_set_object_buffer(request, "os2host32.exe");
            return (Os2SesmgrApiRet)GetLastError();
        }
    } else if (host_len >= (DWORD)sizeof(host)) {
        os2_sesmgr_set_object_buffer(request, "os2host32.exe");
        return OS2_SESMGR_ERROR_INVALID_PARAMETER;
    }

    command = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                O2_SESSION_CMDLINE_MAX);
    if (command == NULL)
        return OS2_SESMGR_ERROR_NOT_ENOUGH_MEMORY;
    if (!append_quoted_arg(command, O2_SESSION_CMDLINE_MAX, host) ||
        !append_text(command, O2_SESSION_CMDLINE_MAX, " --run-quiet ") ||
        !append_quoted_arg(command, O2_SESSION_CMDLINE_MAX,
                           request->program)) {
        HeapFree(GetProcessHeap(), 0, command);
        return OS2_SESMGR_ERROR_INVALID_PARAMETER;
    }
    inputs = request->inputs;
    if (inputs != NULL && *inputs != '\0') {
        if (!append_text(command, O2_SESSION_CMDLINE_MAX, " ") ||
            !append_text(command, O2_SESSION_CMDLINE_MAX, inputs)) {
            HeapFree(GetProcessHeap(), 0, command);
            return OS2_SESMGR_ERROR_INVALID_PARAMETER;
        }
    }

    normalized_env = NULL;
    normalized_bytes = 0U;
    env_rc = OS2_SESMGR_NO_ERROR;
    if (request->environment != NULL) {
        normalized_env = normalize_environment(request->environment,
                                               &normalized_bytes, &env_rc);
        if (normalized_env == NULL) {
            HeapFree(GetProcessHeap(), 0, command);
            return env_rc != OS2_SESMGR_NO_ERROR ? env_rc :
                   OS2_SESMGR_ERROR_INVALID_PARAMETER;
        }
    }

    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    si.lpTitle = (LPSTR)request->title;

    if (request->session_type != OS2_SESMGR_TYPE_FULLSCREEN) {
        if ((request->program_control & OS2_SESMGR_CONTROL_INVISIBLE) != 0U) {
            si.dwFlags |= STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
        } else if ((request->program_control &
                    OS2_SESMGR_CONTROL_MAXIMIZE) != 0U) {
            si.dwFlags |= STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_SHOWMAXIMIZED;
        } else if ((request->program_control &
                    OS2_SESMGR_CONTROL_MINIMIZE) != 0U) {
            si.dwFlags |= STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_SHOWMINIMIZED;
        }
        if ((request->program_control & OS2_SESMGR_CONTROL_SETPOS) != 0U) {
            si.dwFlags |= STARTF_USEPOSITION | STARTF_USESIZE;
            si.dwX = (DWORD)request->init_x;
            si.dwY = (DWORD)request->init_y;
            si.dwXSize = (DWORD)request->init_cx;
            si.dwYSize = (DWORD)request->init_cy;
        }
    }

    creation_flags = CREATE_NEW_PROCESS_GROUP;
    if (request->related == OS2_SESMGR_RELATED_CHILD)
        creation_flags |= CREATE_SUSPENDED;
    if (request->session_type == OS2_SESMGR_TYPE_PM) {
        creation_flags |= DETACHED_PROCESS;
    } else {
        creation_flags |= CREATE_NEW_CONSOLE;
        if (request->fgbg == OS2_SESMGR_FGBG_BACK &&
            (si.dwFlags & STARTF_USESHOWWINDOW) == 0U) {
            si.dwFlags |= STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_SHOWNOACTIVATE;
        }
    }

    if (trace_enabled()) {
        fprintf(stderr,
                "SESMGR START: program=[%s] inputs=[%s] title=[%s] related=%u fgbg=%u type=%u inherit=%u control=0x%04X pos=%u,%u size=%u,%u create=0x%08lX env=%s bytes=%lu\n",
                request->program,
                inputs != NULL ? inputs : "",
                request->title != NULL ? request->title : "",
                (unsigned)request->related, (unsigned)request->fgbg,
                (unsigned)request->session_type,
                (unsigned)request->inherit_opt,
                (unsigned)request->program_control,
                (unsigned)request->init_x, (unsigned)request->init_y,
                (unsigned)request->init_cx, (unsigned)request->init_cy,
                (unsigned long)creation_flags,
                normalized_env != NULL ? "custom" : "inherit",
                (unsigned long)normalized_bytes);
        if (request->icon_file != NULL && *request->icon_file != '\0')
            fprintf(stderr,
                    "SESMGR START: icon=[%s] (recorded; host icon binding not implemented)\n",
                    request->icon_file);
        fprintf(stderr, "SESMGR START: command=[%s]\n", command);
        fflush(stderr);
    }

    ok = CreateProcessA(NULL, command, NULL, NULL, FALSE, creation_flags,
                        normalized_env, NULL, &si, &pi);
    if (normalized_env != NULL)
        HeapFree(GetProcessHeap(), 0, normalized_env);
    HeapFree(GetProcessHeap(), 0, command);
    if (!ok) {
        os2_sesmgr_set_object_buffer(request, request->program);
        return (Os2SesmgrApiRet)GetLastError();
    }

    job = NULL;
    if (request->related == OS2_SESMGR_RELATED_CHILD) {
        job = CreateJobObjectA(NULL, NULL);
        if (job != NULL && !AssignProcessToJobObject(job, pi.hProcess)) {
            CloseHandle(job);
            job = NULL;
        }
    }

    launch->pid = (Os2SesmgrU32)pi.dwProcessId;
    launch->process = (Os2SesmgrNative)(ULONG_PTR)pi.hProcess;
    launch->initial_thread = (Os2SesmgrNative)(ULONG_PTR)pi.hThread;
    launch->group = (Os2SesmgrNative)(ULONG_PTR)job;
    return OS2_SESMGR_NO_ERROR;
}

static Os2SesmgrApiRet win32_resume(void *opaque, Os2SesmgrNative native)
{
    DWORD rc;
    (void)opaque;
    rc = ResumeThread((HANDLE)(ULONG_PTR)native);
    if (rc == (DWORD)-1)
        return (Os2SesmgrApiRet)GetLastError();
    return OS2_SESMGR_NO_ERROR;
}

static void win32_close_thread(void *opaque, Os2SesmgrNative native)
{
    (void)opaque;
    if (native != OS2_SESMGR_NATIVE_INVALID)
        CloseHandle((HANDLE)(ULONG_PTR)native);
}

static Os2SesmgrApiRet win32_terminate(void *opaque, Os2SesmgrNative process,
                                       Os2SesmgrNative group)
{
    BOOL stopped;
    (void)opaque;
    stopped = FALSE;
    if (group != OS2_SESMGR_NATIVE_INVALID)
        stopped = TerminateJobObject((HANDLE)(ULONG_PTR)group, 1U);
    if (!stopped)
        stopped = TerminateProcess((HANDLE)(ULONG_PTR)process, 1U);
    if (!stopped) {
        Os2SesmgrApiRet error;
        error = (Os2SesmgrApiRet)GetLastError();
        return error != 0UL ? error : OS2_SESMGR_ERROR_INVALID_CALL;
    }
    return OS2_SESMGR_NO_ERROR;
}

static void win32_wait_process(void *opaque, Os2SesmgrNative process)
{
    (void)opaque;
    if (process != OS2_SESMGR_NATIVE_INVALID)
        (void)WaitForSingleObject((HANDLE)(ULONG_PTR)process, 5000U);
}

static void win32_close_process(void *opaque, Os2SesmgrNative process,
                                Os2SesmgrNative group)
{
    (void)opaque;
    if (process != OS2_SESMGR_NATIVE_INVALID)
        CloseHandle((HANDLE)(ULONG_PTR)process);
    if (group != OS2_SESMGR_NATIVE_INVALID)
        CloseHandle((HANDLE)(ULONG_PTR)group);
}

static void win32_set_current_title(void *opaque, const char *title)
{
    (void)opaque;
    SetConsoleTitleA(title);
}

static void win32_trace_start(void *opaque, Os2SesmgrU32 sid,
                              Os2SesmgrU32 pid)
{
    (void)opaque;
    if (trace_enabled()) {
        fprintf(stderr, "SESMGR START: session=%lu pid=%lu\n",
                (unsigned long)sid, (unsigned long)pid);
        fflush(stderr);
    }
}

static void win32_trace_stop(void *opaque, Os2SesmgrU32 sid,
                             Os2SesmgrU32 pid)
{
    (void)opaque;
    if (trace_enabled()) {
        fprintf(stderr, "SESMGR STOP: session=%lu pid=%lu\n",
                (unsigned long)sid, (unsigned long)pid);
        fflush(stderr);
    }
}

static const struct Os2SesmgrBackendOps win32_ops = {
    win32_local_lock,
    win32_local_unlock,
    win32_registry_lock,
    win32_registry_unlock,
    win32_current_identity,
    win32_identity_from_process,
    win32_identity_alive,
    win32_owned_process_alive,
    win32_launch,
    win32_resume,
    win32_close_thread,
    win32_terminate,
    win32_wait_process,
    win32_close_process,
    win32_set_current_title,
    win32_trace_start,
    win32_trace_stop
};

int os2_sesmgr_win32_init(struct Os2SesmgrSession *session)
{
    struct Os2SesmgrWin32Context *ctx;
    if (session == NULL)
        return 0;
    ctx = (struct Os2SesmgrWin32Context *)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*ctx));
    if (ctx == NULL)
        return 0;
    InitializeCriticalSection(&ctx->local_lock);
    ctx->local_lock_ready = 1;
    os2_sesmgr_session_init(session, ctx, &win32_ops);
    return 1;
}

void os2_sesmgr_win32_destroy(struct Os2SesmgrSession *session)
{
    struct Os2SesmgrWin32Context *ctx;
    if (session == NULL)
        return;
    ctx = (struct Os2SesmgrWin32Context *)session->backend_opaque;
    if (ctx == NULL)
        return;
    os2_sesmgr_session_destroy(session);
    if (ctx->registry != NULL)
        UnmapViewOfFile(ctx->registry);
    if (ctx->registry_map != NULL)
        CloseHandle(ctx->registry_map);
    if (ctx->registry_mutex != NULL)
        CloseHandle(ctx->registry_mutex);
    if (ctx->local_lock_ready)
        DeleteCriticalSection(&ctx->local_lock);
    HeapFree(GetProcessHeap(), 0, ctx);
    memset(session, 0, sizeof(*session));
}
