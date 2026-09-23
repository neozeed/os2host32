/*
 * doscalls.c - Win32 personality DLL for translated Microsoft C/386 OS/2 LE programs.
 *
 * le2pe386 keeps the original DOSCALLS ordinal imports.  This DLL supplies a
 * gradually growing subset of the 32-bit OS/2 Control Program API on Win32.
 *
 * Milestone 4 added the file-manager calls used by the Info TaskForce '87
 * parser specimen and maps OS/2's small integer HFILE values to native Win32
 * HANDLEs.  Milestone 7 added DosGetDateTime (ordinal 230), used by phoon.
 * Milestone 8 adds DosCreateDir (270) and DosQueryFileInfo (279), used by
 * the NULL/headless Doom specimen.  Milestone 13 added a synchronous DosBeep
 * implemented with WinMM waveOut.  Milestone 14 corrects the 32-bit C/386
 * export ordinal to DOSCALLS.286 (DOS16BEEP remains ordinal 50 in real OS/2).
 * Milestone 17 keeps the waveOut device open, waits on a completion event
 * instead of Sleep(1) polling, reuses the sample buffer, and preserves phase
 * between adjacent notes.  This removes the large per-note gaps heard on
 * short Sarien beeper notes (the issue was audio-device churn, not CPU speed).
 *
 * This source intentionally stays close to ANSI C89 for old Microsoft C/C++
 * compiler experiments.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "os2_doscalls_core.h"
#include "os2_win32_services.h"
#include "os2_nls.h"
#include "os2_nls_api.h"

#ifndef __cdecl
#define __cdecl
#endif

typedef DWORD O2APIRET;
typedef DWORD O2ULONG;
typedef LONG  O2LONG;
typedef DWORD O2HFILE;

/*
 * M29B C/386 far16 bridge helper.
 *
 * Microsoft C/386 6.00.081 passes the flat pointer in EAX and expects
 * DosFlatToSel to return a packed 16:16 value in EAX.  OS2HOST32 never loads
 * this value into a segment register: the generated 32->16 helper is patched
 * to a native Win32 bridge before execution.  Therefore the original flat
 * pointer itself is a sufficient virtual far-pointer token.
 *
 * Keep EAX untouched and return immediately.
 */
#if defined(__GNUC__) && defined(__i386__)
void __attribute__((naked)) __cdecl DosFlatToSel(void)
{
    __asm__ __volatile__("ret");
}
#elif defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) void __cdecl DosFlatToSel(void)
{
    __asm ret
}
#else
/*
 * This DLL is intended for 32-bit x86.  The fallback is only here to keep
 * source parsers happy on other hosts; it does not implement the register ABI.
 */
void __cdecl DosFlatToSel(void)
{
}
#endif

/*
 * M30 EMX uses the inverse selector helper as well.  The native EMX bridge
 * never materializes a real Win32 selector: both directions carry the flat
 * address as a token in EAX, so DosSelToFlat is the same register-ABI no-op.
 */
#if defined(__GNUC__) && defined(__i386__)
void __attribute__((naked)) __cdecl DosSelToFlat(void)
{
    __asm__ __volatile__("ret");
}
#elif defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) void __cdecl DosSelToFlat(void)
{
    __asm ret
}
#else
void __cdecl DosSelToFlat(void)
{
}
#endif

/* DosExecPgm execution modes used by the direct-LX process gate. */
#define O2_EXEC_SYNC         0UL
#define O2_EXEC_ASYNC        1UL
#define O2_EXEC_ASYNCRESULT  2UL
#define O2_EXEC_BACKGROUND   4UL

#define O2_DCWA_PROCESS 0UL
#define O2_DCWW_WAIT    0UL
#define O2_DCWW_NOWAIT  1UL

/* OS/2 RESULTCODES termination reasons. */
#define O2_TC_EXIT         0UL
#define O2_TC_HARDERROR    1UL
#define O2_TC_TRAP         2UL
#define O2_TC_KILLPROCESS  3UL
#define O2_TC_EXCEPTION    4UL

/* Win32 uses this NT status when an unhandled console Ctrl+C terminates a
 * process.  Translate it at the personality boundary instead of leaking the
 * host-specific 0xC000013A value into an OS/2 RESULTCODES structure. */
#define O2_WIN_STATUS_CONTROL_C_EXIT 0xC000013AUL

struct O2ResultCodes {
    O2ULONG codeTerminate;
    O2ULONG codeResult;
};

#define O2_EXEC_CMDLINE_MAX 4096U

#define O2_NO_ERROR                 0UL
#define O2_ERROR_INVALID_FUNCTION    1UL
#define O2_ERROR_TOO_MANY_OPEN_FILES 4UL
#define O2_ERROR_INVALID_HANDLE     6UL
#define O2_ERROR_INVALID_ACCESS    12UL
#define O2_ERROR_NOT_ENOUGH_MEMORY  8UL
#define O2_ERROR_INVALID_PARAMETER 87UL
#define O2_ERROR_INVALID_CATEGORY 117UL
#define O2_ERROR_WAIT_NO_CHILDREN 128UL
#define O2_ERROR_CHILD_NOT_COMPLETE 129UL
#define O2_ERROR_BUFFER_OVERFLOW 111UL
#define O2_ERROR_INVALID_TARGET_HANDLE 114UL
#define O2_ERROR_ENVVAR_NOT_FOUND 203UL
#define O2_ERROR_MOD_NOT_FOUND    126UL
#define O2_ERROR_INVALID_EXE_SIGNATURE 191UL
#define O2_ERROR_PROC_NOT_FOUND   127UL
#define O2_ERROR_INIT_ROUTINE_FAILED 295UL
#define O2_ERROR_INVALID_FREQUENCY 395UL

/* OS/2 semaphore status codes used by the EMX runtime. */
#define O2_ERROR_DUPLICATE_NAME    285UL
#define O2_ERROR_TOO_MANY_OPENS    291UL
#define O2_ERROR_SEM_NOT_FOUND      187UL
#define O2_ERROR_ALREADY_POSTED    299UL
#define O2_ERROR_ALREADY_RESET     300UL

/*
 * M30 first-pass signal surface used through EMX's generic far16 wrapper.
 * Dhrystone only needs EMX to install its runtime handlers; Win32 console
 * control handling remains owned by the personality layer.  Preserve the
 * previous-handler outputs conservatively and accept the registration.
 */
unsigned short __cdecl DosSetSigHandler(void *routine, void *prev_address,
                                         unsigned short *prev_action,
                                         unsigned short action,
                                         unsigned short sig_number)
{
    (void)routine;
    (void)action;
    (void)sig_number;
    if (prev_address != NULL)
        *(O2ULONG *)prev_address = 0UL;
    if (prev_action != NULL)
        *prev_action = 0U;
    return (unsigned short)O2_NO_ERROR;
}

unsigned short __cdecl DosFlagProcess(unsigned short process_id,
                                      unsigned short action_code,
                                      unsigned short flag_number,
                                      unsigned short flag_argument)
{
    (void)process_id;
    (void)action_code;
    (void)flag_number;
    (void)flag_argument;
    return (unsigned short)O2_NO_ERROR;
}

/* OS/2 page flags used by this runtime. */
#define O2_PAG_READ       0x00000001UL
#define O2_PAG_WRITE      0x00000002UL
#define O2_PAG_EXECUTE    0x00000004UL
#define O2_PAG_GUARD      0x00000008UL
#define O2_PAG_COMMIT     0x00000010UL
#define O2_PAG_DECOMMIT   0x00000020UL
#define O2_PAG_FREE       0x00004000UL
#define O2_PAG_BASE       0x00010000UL

/* OS/2 32-bit memory-suballocation flags.  EMX uses this family for its
 * small-block heap.  INIT/GROW are the only flags which affect M30's
 * external bookkeeping; sparse objects are already committed by our
 * DosAllocMem and SERIALIZE is naturally process-local here. */
#define O2_DOSSUB_INIT       0x00000001UL
#define O2_DOSSUB_GROW       0x00000002UL
#define O2_DOSSUB_SPARSE_OBJ 0x00000004UL
#define O2_DOSSUB_SERIALIZE  0x00000008UL
#define O2_SUBPOOL_HEADER    64UL
#define O2_MAX_SUBPOOLS      32
#define O2_MAX_SUBRANGES     256

/* OS/2 file attributes. */
#define O2_FILE_READONLY   0x0001UL
#define O2_FILE_HIDDEN     0x0002UL
#define O2_FILE_SYSTEM     0x0004UL
#define O2_FILE_DIRECTORY  0x0010UL
#define O2_FILE_ARCHIVED   0x0020UL

/* DosOpen action flags. */
#define O2_OPEN_ACTION_FAIL_IF_EXISTS     0x0000UL
#define O2_OPEN_ACTION_OPEN_IF_EXISTS     0x0001UL
#define O2_OPEN_ACTION_REPLACE_IF_EXISTS  0x0002UL
#define O2_OPEN_ACTION_FAIL_IF_NEW        0x0000UL
#define O2_OPEN_ACTION_CREATE_IF_NEW      0x0010UL

/* DosOpen access/share fields. */
#define O2_OPEN_ACCESS_READONLY   0x0000UL
#define O2_OPEN_ACCESS_WRITEONLY  0x0001UL
#define O2_OPEN_ACCESS_READWRITE  0x0002UL
#define O2_OPEN_SHARE_DENYREADWRITE 0x0010UL
#define O2_OPEN_SHARE_DENYWRITE     0x0020UL
#define O2_OPEN_SHARE_DENYREAD      0x0030UL
#define O2_OPEN_SHARE_DENYNONE      0x0040UL

#define O2_FIL_STANDARD 1UL
#define O2_FIL_QUERYFULLNAME 5UL

#define O2_MAX_HANDLES 256
#define O2_MAX_FIND_HANDLES 64
#define O2_MAX_CHILDREN 64

#define O2_ERROR_NO_MORE_FILES      18UL
#define O2_SEM_INDEFINITE_WAIT      0xffffffffUL

typedef void (__cdecl *O2THREADFN)(O2ULONG);

struct O2ThreadStart {
    O2THREADFN fn;
    O2ULONG arg;
};

static HANDLE o2_handles[O2_MAX_HANDLES];
/* OS/2 2.x starts with a small per-process HFILE ceiling and lets programs
 * adjust it with DosSetRelMaxFH.  Keep a logical ceiling inside our fixed
 * 256-slot compatibility table. */
static O2ULONG o2_max_file_handles = 20;
/*
 * OS/2 HFILE 0/1/2 need their own owned HANDLEs once redirected.  A boolean
 * ownership flag is not enough because the native CMD bootstrap also mirrors
 * the selected handle into its CRT fd table.  Track the exact HANDLE that
 * belongs to DOSCALLS so CRT SetStdHandle/_dup2 activity cannot make us close
 * the wrong object on the next DosDupHandle.
 *
 * NULL means "use the process's original Win32 standard handle".
 */
static HANDLE o2_std_handles[3];
static HANDLE o2_find_handles[O2_MAX_FIND_HANDLES];

struct O2ChildProc {
    HANDLE process;
    DWORD pid;
};
static struct O2ChildProc o2_children[O2_MAX_CHILDREN];
static CRITICAL_SECTION child_lock;
static int child_lock_ready;

/* M30I: OS/2 DosSub* small-block pools.  The real kernel stores allocator
 * bookkeeping in the first 64 bytes of the supplied object.  Keep metadata
 * on the host so we do not expose host pointers to guest code, while
 * preserving the observable OS/2 layout: first allocation begins at +64 and
 * each requested block is rounded to an 8-byte boundary. */
struct O2SubRange {
    O2ULONG off;
    O2ULONG len;
    int is_free;
};

struct O2SubPool {
    void *base;
    O2ULONG size;
    O2ULONG flags;
    int in_use;
    unsigned int nranges;
    struct O2SubRange ranges[O2_MAX_SUBRANGES];
};

static struct O2SubPool o2_subpools[O2_MAX_SUBPOOLS];

/* Minimal 32-bit OS/2 TIB/PIB views for DosGetInfoBlocks.  The direct LX
 * backend currently needs pib_pchenv; the remaining fields are populated
 * conservatively so other simple clients can inspect them too. */
struct O2TibCompat {
    void *pexchain;
    void *pstack;
    void *pstacklimit;
    void *ptib2;
    O2ULONG version;
    O2ULONG ordinal;
};

struct O2Tib2Compat {
    O2ULONG tid;
    O2ULONG priority;
    O2ULONG version;
    unsigned short mc_count;
    unsigned short mc_force;
};

struct O2PibCompat {
    O2ULONG pid;
    O2ULONG ppid;
    O2ULONG hmte;
    char *pchcmd;
    char *pchenv;
    O2ULONG flstatus;
    O2ULONG ultype;
};

#define O2_INFO_ENV_MAX 32768U
#define O2_INFO_CMD_MAX 65536U
static struct O2TibCompat o2_info_tib;
static struct O2Tib2Compat o2_info_tib2;
static struct O2PibCompat o2_info_pib;

/*
 * M30E: minimal OS/2 32-bit exception-registration chain.  Real OS/2 keeps
 * the head in the current thread's TIB and DosSetExceptionHandler links the
 * caller-supplied two-dword record at the front.  M30 is intentionally
 * single-threaded for EMX, so one process-local chain is sufficient for the
 * current guest.  0xffffffff is the historical end-of-chain marker.
 *
 * We do not deliver Win32 faults through this chain yet; this milestone only
 * supplies the registration semantics EMX requires during normal startup.
 */
struct O2ExceptionRegistrationRecord {
    struct O2ExceptionRegistrationRecord *prev_structure;
    void *exception_handler;
};
static struct O2ExceptionRegistrationRecord *o2_exception_head =
    (struct O2ExceptionRegistrationRecord *)(ULONG_PTR)0xffffffffUL;

/* M30F: process-level Ctrl-C/Ctrl-Break signal-exception focus nesting.
 * OS/2 exposes this as a get/release count through DosSetSignalExceptionFocus.
 * The current host is single-console/single-process, so no session-manager
 * arbitration is needed yet; retaining the count preserves the API contract
 * EMX checks during startup/teardown. */
static O2ULONG o2_signal_exception_focus_count;
static char o2_info_env[O2_INFO_ENV_MAX];
static char o2_info_cmd[O2_INFO_CMD_MAX];
static char o2_scan_env[O2_INFO_ENV_MAX];

/* M30D private in-process bridge: ask OS2HOST32.EXE for the mapped guest
 * main-thread stack.  Keep this separate from the module-manager bridge so
 * older DOSCALLS/module API behavior is unchanged. */
static int o2_query_main_guest_stack(O2ULONG *plow, O2ULONG *phigh)
{
    typedef O2APIRET (__cdecl *QUERYFN)(O2ULONG *, O2ULONG *);
    HMODULE exe;
    QUERYFN fn;

    if (!plow || !phigh)
        return 0;
    exe = GetModuleHandleA(NULL);
    if (!exe)
        return 0;
    fn = (QUERYFN)GetProcAddress(exe, "OS2HostQueryMainStack");
    if (!fn)
        return 0;
    return fn(plow, phigh) == O2_NO_ERROR;
}

/* M30M2: ask the loader for the exact OS/2 PIB command block.  It contains
 * embedded NULs: argv0\0command-tail\0\0. */
static int o2_query_main_guest_command(char **ppcmd, O2ULONG *pcb)
{
    typedef O2APIRET (__cdecl *QUERYFN)(char **, O2ULONG *);
    HMODULE exe;
    QUERYFN fn;

    if (!ppcmd || !pcb)
        return 0;
    *ppcmd = NULL;
    *pcb = 0;
    exe = GetModuleHandleA(NULL);
    if (!exe)
        return 0;
    fn = (QUERYFN)GetProcAddress(exe, "OS2HostQueryMainCommand");
    if (!fn)
        return 0;
    return fn(ppcmd, pcb) == O2_NO_ERROR && *ppcmd != NULL && *pcb >= 3UL;
}

/* Persistent synchronous waveOut backend for DosBeep.  DosBeep itself is
 * serialized, just as a single PC-style tone generator would be. */
static CRITICAL_SECTION beep_lock;
static int beep_lock_ready;
static HANDLE beep_event;
static HWAVEOUT beep_wave;
static short *beep_samples;
static DWORD beep_sample_capacity;
static O2ULONG beep_phase;

static int audio_trace_enabled(void)
{
    char b[4];
    return GetEnvironmentVariableA("LE2PE_TRACE_AUDIO", b, sizeof(b)) != 0;
}

/* M28D5 diagnostic switch.  Keep tracing behind an environment variable so
 * normal guests see no extra output.  This deliberately traces the OS/2 HFILE
 * number and the native HANDLE selected by the personality; it lets us tell
 * whether a C/386 stdio failure happens before DosWrite, inside DosWrite, or
 * because HFILE 0/1 were resolved to the wrong Windows objects. */
static int io_trace_enabled(void)
{
    char b[4];
    return GetEnvironmentVariableA("OS2_TRACE_IO", b, sizeof(b)) != 0;
}

/* M30B: focused trace for EMX inspecting its bound executable.  This is
 * intentionally separate from the older general I/O trace so a normal guest
 * is not made noisy unless the test driver explicitly asks for it. */
static int emx_self_trace_enabled(void)
{
    char b[4];
    return GetEnvironmentVariableA("OS2_TRACE_EMX_SELF", b, sizeof(b)) != 0;
}

static void trace_hex16(const unsigned char *p, O2ULONG n)
{
    O2ULONG i;
    O2ULONG lim;
    lim = n < 16UL ? n : 16UL;
    for (i = 0; i < lim; ++i)
        fprintf(stderr, "%s%02X", i ? " " : "", (unsigned)p[i]);
    if (n > lim)
        fprintf(stderr, " ...");
}

/*
 * M29J2 process-launch tracing.  Keep it opt-in because DosExecPgm is used by
 * normal guests as well as the reconstructed shell.
 */
static void o2_module_process_exit(void);

static int exec_trace_enabled(void)
{
    char b[4];
    return GetEnvironmentVariableA("OS2_TRACE_EXEC", b, sizeof(b)) != 0;
}

static const char *win_handle_type_name(HANDLE h)
{
    DWORD t;
    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return "invalid";
    t = GetFileType(h);
    if (t == FILE_TYPE_DISK)
        return "disk";
    if (t == FILE_TYPE_CHAR)
        return "char";
    if (t == FILE_TYPE_PIPE)
        return "pipe";
    if (t == FILE_TYPE_UNKNOWN)
        return "unknown";
    return "other";
}


static HANDLE os2_handle(O2HFILE h)
{
    if (h <= 2UL) {
        DWORD which;
        if (o2_std_handles[h] != NULL &&
            o2_std_handles[h] != INVALID_HANDLE_VALUE)
            return o2_std_handles[h];
        if (h == 0UL)
            which = STD_INPUT_HANDLE;
        else if (h == 1UL)
            which = STD_OUTPUT_HANDLE;
        else
            which = STD_ERROR_HANDLE;
        return GetStdHandle(which);
    }
    if (h >= O2_MAX_HANDLES)
        return INVALID_HANDLE_VALUE;
    if (o2_handles[h] == NULL)
        return INVALID_HANDLE_VALUE;
    return o2_handles[h];
}

static O2HFILE alloc_os2_handle(HANDLE h)
{
    O2HFILE i;
    for (i = 3; i < o2_max_file_handles && i < O2_MAX_HANDLES; ++i) {
        if (o2_handles[i] == NULL) {
            o2_handles[i] = h;
            return i;
        }
    }
    return (O2HFILE)0xffffffffUL;
}

static void free_os2_handle(O2HFILE h)
{
    if (h >= 3 && h < O2_MAX_HANDLES)
        o2_handles[h] = NULL;
}


static O2ULONG alloc_find_handle(HANDLE h)
{
    O2ULONG i;
    for (i = 1; i < O2_MAX_FIND_HANDLES; ++i) {
        if (o2_find_handles[i] == NULL) {
            o2_find_handles[i] = h;
            return i;
        }
    }
    return 0xffffffffUL;
}

static HANDLE get_find_handle(O2ULONG hdir)
{
    if (hdir == 0 || hdir >= O2_MAX_FIND_HANDLES)
        return INVALID_HANDLE_VALUE;
    if (o2_find_handles[hdir] == NULL)
        return INVALID_HANDLE_VALUE;
    return o2_find_handles[hdir];
}

static void free_find_handle(O2ULONG hdir)
{
    if (hdir > 0 && hdir < O2_MAX_FIND_HANDLES)
        o2_find_handles[hdir] = NULL;
}

static int store_child_process(HANDLE process, DWORD pid)
{
    int i;

    if (!child_lock_ready || process == NULL ||
        process == INVALID_HANDLE_VALUE || pid == 0)
        return 0;

    EnterCriticalSection(&child_lock);
    for (i = 0; i < O2_MAX_CHILDREN; ++i) {
        if (o2_children[i].process == NULL) {
            o2_children[i].process = process;
            o2_children[i].pid = pid;
            LeaveCriticalSection(&child_lock);
            return 1;
        }
    }
    LeaveCriticalSection(&child_lock);
    return 0;
}

static HANDLE take_child_process(DWORD wantedPid, DWORD *actualPid)
{
    HANDLE process;
    int i;

    process = NULL;
    if (actualPid != NULL)
        *actualPid = 0;
    if (!child_lock_ready)
        return NULL;

    EnterCriticalSection(&child_lock);
    for (i = 0; i < O2_MAX_CHILDREN; ++i) {
        if (o2_children[i].process != NULL &&
            (wantedPid == 0 || o2_children[i].pid == wantedPid)) {
            process = o2_children[i].process;
            if (actualPid != NULL)
                *actualPid = o2_children[i].pid;
            o2_children[i].process = NULL;
            o2_children[i].pid = 0;
            break;
        }
    }
    LeaveCriticalSection(&child_lock);
    return process;
}

static void put_child_process_back(HANDLE process, DWORD pid)
{
    if (process != NULL && process != INVALID_HANDLE_VALUE)
        (void)store_child_process(process, pid);
}

static DWORD protect_from_flags(O2ULONG flags)
{
    int r;
    int w;
    int x;

    r = (flags & O2_PAG_READ) != 0;
    w = (flags & O2_PAG_WRITE) != 0;
    x = (flags & O2_PAG_EXECUTE) != 0;

    if (x && w)
        return PAGE_EXECUTE_READWRITE;
    if (x && r)
        return PAGE_EXECUTE_READ;
    if (x)
        return PAGE_EXECUTE;
    if (w)
        return PAGE_READWRITE;
    if (r)
        return PAGE_READONLY;
    return PAGE_NOACCESS;
}

/*
 * Shared-personality native adapter.
 *
 * In the transformed Win32 process, a 32-bit OS/2 linear address is also a
 * directly usable process address.  The common implementation still carries
 * it as an explicit os2_addr32_t so the same code can be linked into the
 * Win64/WHP loader, where the address must be checked against guest RAM.
 */
static os2_api_ret_t o2p_validate_memory(void *opaque,
                                         os2_addr32_t address,
                                         uint32_t length,
                                         uint32_t access)
{
    (void)opaque;
    (void)access;
    if (length != 0u && address == 0u)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    if (length != 0u && address > 0xffffffffu - (length - 1u))
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t o2p_write_memory(void *opaque,
                                      os2_addr32_t address,
                                      const void *source,
                                      uint32_t length)
{
    (void)opaque;
    if (length != 0u)
        memcpy((void *)(ULONG_PTR)address, source, (size_t)length);
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t o2p_map_read_memory(void *opaque,
                                         os2_addr32_t address,
                                         uint32_t length,
                                         const void **host_pointer)
{
    (void)opaque;
    (void)length;
    if (host_pointer == NULL)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;
    *host_pointer = (const void *)(ULONG_PTR)address;
    return OS2_PERSONALITY_NO_ERROR;
}

static os2_api_ret_t o2p_query_handle_type(void *opaque,
                                           os2_handle32_t hFile,
                                           uint32_t *type,
                                           uint32_t *attributes)
{
    DWORD native_type;
    HANDLE h;
    (void)opaque;

    h = os2_handle((O2HFILE)hFile);
    if (h == INVALID_HANDLE_VALUE)
        return O2_ERROR_INVALID_HANDLE;
    native_type = GetFileType(h);
    if (native_type == FILE_TYPE_CHAR)
        *type = 1u;
    else if (native_type == FILE_TYPE_PIPE)
        *type = 2u;
    else
        *type = 0u;
    *attributes = 0u;
    if (io_trace_enabled()) {
        fprintf(stderr,
                "DOSCALLS IO: QHTYPE hfile=%lu handle=%p native=%s -> type=%lu attr=%lu\n",
                (unsigned long)hFile, (void *)h, win_handle_type_name(h),
                (unsigned long)*type, (unsigned long)*attributes);
        fflush(stderr);
    }
    return O2_NO_ERROR;
}

static os2_api_ret_t o2p_write_handle(void *opaque,
                                      os2_handle32_t hFile,
                                      const void *buffer,
                                      uint32_t count,
                                      uint32_t *actual)
{
    DWORD done;
    DWORD error;
    HANDLE h;
    const void *native_buffer;
    (void)opaque;

    h = os2_handle((O2HFILE)hFile);
    if (h == INVALID_HANDLE_VALUE)
        return O2_ERROR_INVALID_HANDLE;
    done = 0;
    native_buffer = count != 0u ? buffer : (const void *)"";
    if (!WriteFile(h, native_buffer, (DWORD)count, &done, NULL)) {
        error = GetLastError();
        *actual = (uint32_t)done;
        if (io_trace_enabled()) {
            fprintf(stderr,
                    "DOSCALLS IO: WRITE hfile=%lu handle=%p native=%s ask=%lu -> err=%lu actual=%lu\n",
                    (unsigned long)hFile, (void *)h, win_handle_type_name(h),
                    (unsigned long)count, (unsigned long)error,
                    (unsigned long)*actual);
            fflush(stderr);
        }
        return (os2_api_ret_t)error;
    }
    *actual = (uint32_t)done;
    if (io_trace_enabled()) {
        fprintf(stderr,
                "DOSCALLS IO: WRITE hfile=%lu handle=%p native=%s ask=%lu -> rc=0 actual=%lu\n",
                (unsigned long)hFile, (void *)h, win_handle_type_name(h),
                (unsigned long)count, (unsigned long)*actual);
        fflush(stderr);
    }
    return O2_NO_ERROR;
}

static os2_api_ret_t o2p_set_file_pointer(void *opaque,
                                          os2_handle32_t hFile,
                                          os2_long32_t distance,
                                          uint32_t method,
                                          uint32_t *new_position)
{
    DWORD native_method;
    DWORD result;
    DWORD error;
    HANDLE h;
    (void)opaque;

    native_method = method == 0u ? FILE_BEGIN :
                    (method == 1u ? FILE_CURRENT : FILE_END);
    h = os2_handle((O2HFILE)hFile);
    if (h == INVALID_HANDLE_VALUE)
        return O2_ERROR_INVALID_HANDLE;
    SetLastError(NO_ERROR);
    result = SetFilePointer(h, (LONG)distance, NULL, native_method);
    if (result == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
        error = GetLastError();
        if (io_trace_enabled()) {
            fprintf(stderr,
                    "DOSCALLS IO: SEEK hfile=%lu handle=%p native=%s dist=%ld method=%lu -> err=%lu\n",
                    (unsigned long)hFile, (void *)h, win_handle_type_name(h),
                    (long)distance, (unsigned long)method,
                    (unsigned long)error);
            fflush(stderr);
        }
        return (os2_api_ret_t)error;
    }
    *new_position = (uint32_t)result;
    if (emx_self_trace_enabled()) {
        fprintf(stderr,
                "M30B SELF: DosSetFilePtr hfile=%lu dist=%ld method=%lu -> rc=0 pos=%lu\n",
                (unsigned long)hFile, (long)distance,
                (unsigned long)method, (unsigned long)*new_position);
        fflush(stderr);
    }
    if (io_trace_enabled()) {
        fprintf(stderr,
                "DOSCALLS IO: SEEK hfile=%lu handle=%p native=%s dist=%ld method=%lu -> pos=%lu\n",
                (unsigned long)hFile, (void *)h, win_handle_type_name(h),
                (long)distance, (unsigned long)method,
                (unsigned long)*new_position);
        fflush(stderr);
    }
    return O2_NO_ERROR;
}

static os2_api_ret_t o2p_allocate_memory(void *opaque,
                                         uint32_t size,
                                         uint32_t flags,
                                         uint32_t reserved,
                                         os2_addr32_t *base)
{
    void *p;
    DWORD protect;
    (void)opaque;
    (void)reserved;

    protect = protect_from_flags((O2ULONG)flags);
    if (protect == PAGE_NOACCESS)
        protect = PAGE_READWRITE;
    p = VirtualAlloc(NULL, (SIZE_T)size, MEM_RESERVE | MEM_COMMIT, protect);
    if (p == NULL)
        return (os2_api_ret_t)GetLastError();
    *base = (os2_addr32_t)(ULONG_PTR)p;
    return O2_NO_ERROR;
}

static os2_api_ret_t o2p_free_memory(void *opaque, os2_addr32_t base)
{
    (void)opaque;
    if (!VirtualFree((void *)(ULONG_PTR)base, 0, MEM_RELEASE))
        return (os2_api_ret_t)GetLastError();
    return O2_NO_ERROR;
}

static os2_api_ret_t o2p_set_memory(void *opaque,
                                    os2_addr32_t base,
                                    uint32_t size,
                                    uint32_t flags)
{
    void *address;
    DWORD old_protect;
    DWORD protect;
    (void)opaque;

    address = (void *)(ULONG_PTR)base;
    if (flags & O2_PAG_DECOMMIT) {
        if (!VirtualFree(address, (SIZE_T)size, MEM_DECOMMIT))
            return (os2_api_ret_t)GetLastError();
        return O2_NO_ERROR;
    }
    if (flags & O2_PAG_COMMIT) {
        protect = protect_from_flags((O2ULONG)flags);
        if (protect == PAGE_NOACCESS)
            protect = PAGE_READWRITE;
        if (VirtualAlloc(address, (SIZE_T)size, MEM_COMMIT, protect) == NULL)
            return (os2_api_ret_t)GetLastError();
        return O2_NO_ERROR;
    }
    protect = protect_from_flags((O2ULONG)flags);
    if (!VirtualProtect(address, (SIZE_T)size, protect, &old_protect))
        return (os2_api_ret_t)GetLastError();
    return O2_NO_ERROR;
}

static const struct Os2PersonalityOps o2_personality_ops = {
    o2p_validate_memory,
    o2p_write_memory,
    o2p_map_read_memory,
    o2p_query_handle_type,
    o2p_write_handle,
    o2p_set_file_pointer,
    o2p_allocate_memory,
    o2p_free_memory,
    o2p_set_memory,
    os2_win32_query_local_datetime,
    os2_win32_monotonic_milliseconds
};

static struct Os2NlsState o2_nls_state;
static struct Os2PersonalityContext o2_personality_context = {
    NULL,
    &o2_personality_ops,
    &o2_nls_state
};

static os2_addr32_t o2_pointer_address(const void *pointer)
{
    return (os2_addr32_t)(ULONG_PTR)pointer;
}

static O2ULONG o2_attrs_from_win32(DWORD a)
{
    O2ULONG r;
    r = 0;
    if (a & FILE_ATTRIBUTE_READONLY)
        r |= O2_FILE_READONLY;
    if (a & FILE_ATTRIBUTE_HIDDEN)
        r |= O2_FILE_HIDDEN;
    if (a & FILE_ATTRIBUTE_SYSTEM)
        r |= O2_FILE_SYSTEM;
    if (a & FILE_ATTRIBUTE_DIRECTORY)
        r |= O2_FILE_DIRECTORY;
    if (a & FILE_ATTRIBUTE_ARCHIVE)
        r |= O2_FILE_ARCHIVED;
    return r;
}

static DWORD win32_attrs_from_o2(O2ULONG a)
{
    DWORD r;
    r = 0;
    if (a & O2_FILE_READONLY)
        r |= FILE_ATTRIBUTE_READONLY;
    if (a & O2_FILE_HIDDEN)
        r |= FILE_ATTRIBUTE_HIDDEN;
    if (a & O2_FILE_SYSTEM)
        r |= FILE_ATTRIBUTE_SYSTEM;
    if (a & O2_FILE_ARCHIVED)
        r |= FILE_ATTRIBUTE_ARCHIVE;
    if (r == 0)
        r = FILE_ATTRIBUTE_NORMAL;
    return r;
}

static WORD o2_date_from_filetime(const FILETIME *ft, WORD *ptime)
{
    FILETIME lft;
    SYSTEMTIME st;
    unsigned year;
    WORD d;
    WORD t;

    *ptime = 0;
    if (!FileTimeToLocalFileTime(ft, &lft))
        return 0;
    if (!FileTimeToSystemTime(&lft, &st))
        return 0;

    year = st.wYear;
    if (year < 1980)
        year = 1980;
    if (year > 2107)
        year = 2107;

    d = (WORD)((st.wDay & 31U) |
               ((st.wMonth & 15U) << 5) |
               (((year - 1980U) & 127U) << 9));
    t = (WORD)(((st.wSecond / 2U) & 31U) |
               ((st.wMinute & 63U) << 5) |
               ((st.wHour & 31U) << 11));
    *ptime = t;
    return d;
}

static void put16(unsigned char *p, WORD v)
{
    p[0] = (unsigned char)(v & 0xffU);
    p[1] = (unsigned char)((v >> 8) & 0xffU);
}

static void put32(unsigned char *p, DWORD v)
{
    p[0] = (unsigned char)(v & 0xffUL);
    p[1] = (unsigned char)((v >> 8) & 0xffUL);
    p[2] = (unsigned char)((v >> 16) & 0xffUL);
    p[3] = (unsigned char)((v >> 24) & 0xffUL);
}

static WORD get16(const unsigned char *p)
{
    return (WORD)((WORD)p[0] | ((WORD)p[1] << 8));
}

static DWORD get32(const unsigned char *p)
{
    return (DWORD)p[0] | ((DWORD)p[1] << 8) |
           ((DWORD)p[2] << 16) | ((DWORD)p[3] << 24);
}

static int o2_filetime_from_date_time(WORD d, WORD t, FILETIME *ft)
{
    SYSTEMTIME st;
    FILETIME localft;
    unsigned year;

    if (!ft)
        return 0;
    memset(&st, 0, sizeof(st));
    year = 1980U + ((d >> 9) & 127U);
    st.wYear = (WORD)year;
    st.wMonth = (WORD)((d >> 5) & 15U);
    st.wDay = (WORD)(d & 31U);
    st.wHour = (WORD)((t >> 11) & 31U);
    st.wMinute = (WORD)((t >> 5) & 63U);
    st.wSecond = (WORD)((t & 31U) * 2U);
    if (st.wMonth == 0 || st.wDay == 0 || st.wHour > 23 ||
        st.wMinute > 59 || st.wSecond > 59)
        return 0;
    if (!SystemTimeToFileTime(&st, &localft))
        return 0;
    if (!LocalFileTimeToFileTime(&localft, ft))
        return 0;
    return 1;
}

/* FILESTATUS3 / FIL_STANDARD: 24 bytes in the 32-bit API. */
static void fill_fil_standard(unsigned char *p,
                              const WIN32_FIND_DATAA *fd)
{
    WORD d;
    WORD t;
    DWORD size;
    DWORD alloc;
    O2ULONG attr;

    d = o2_date_from_filetime(&fd->ftCreationTime, &t);
    put16(p + 0, d); put16(p + 2, t);
    d = o2_date_from_filetime(&fd->ftLastAccessTime, &t);
    put16(p + 4, d); put16(p + 6, t);
    d = o2_date_from_filetime(&fd->ftLastWriteTime, &t);
    put16(p + 8, d); put16(p + 10, t);

    size = fd->nFileSizeLow;
    alloc = (size + 4095UL) & ~4095UL;
    if (alloc < size)
        alloc = size;
    attr = o2_attrs_from_win32(fd->dwFileAttributes);

    put32(p + 12, size);
    put32(p + 16, alloc);
    put32(p + 20, attr);
}


/* FILESTATUS3 / FIL_STANDARD from an already-open Win32 handle. */
static O2APIRET fill_fil_standard_handle(unsigned char *p, HANDLE h)
{
    BY_HANDLE_FILE_INFORMATION fi;
    WORD d;
    WORD t;
    DWORD size;
    DWORD alloc;
    O2ULONG attr;

    if (!GetFileInformationByHandle(h, &fi))
        return (O2APIRET)GetLastError();

    d = o2_date_from_filetime(&fi.ftCreationTime, &t);
    put16(p + 0, d); put16(p + 2, t);
    d = o2_date_from_filetime(&fi.ftLastAccessTime, &t);
    put16(p + 4, d); put16(p + 6, t);
    d = o2_date_from_filetime(&fi.ftLastWriteTime, &t);
    put16(p + 8, d); put16(p + 10, t);

    /* The C/386-era OS/2 interface is 32-bit here.  These specimens do not
       deal with >4 GiB files, so report the low 32 bits just as that ABI can. */
    size = fi.nFileSizeLow;
    alloc = (size + 4095UL) & ~4095UL;
    if (alloc < size)
        alloc = size;
    attr = o2_attrs_from_win32(fi.dwFileAttributes);

    put32(p + 12, size);
    put32(p + 16, alloc);
    put32(p + 20, attr);
    return O2_NO_ERROR;
}

static DWORD WINAPI o2_thread_start(LPVOID pv)
{
    struct O2ThreadStart *ts;
    O2THREADFN fn;
    O2ULONG arg;

    ts = (struct O2ThreadStart *)pv;
    fn = ts->fn;
    arg = ts->arg;
    HeapFree(GetProcessHeap(), 0, ts);
    fn(arg);
    return 0;
}

static void fill_findbuf_beta2(unsigned char *p, O2ULONG cb,
                                const WIN32_FIND_DATAA *fd)
{
    WORD d, t;
    DWORD size, alloc;
    O2ULONG attr;
    size_t n, room;
    /* Microsoft/IBM OS/2 2.0 Beta-2 FILEFINDBUF is the original packed
       279-byte layout used by OPENDLG.DLL: no oNextEntryOffset and a
       USHORT attrFile. */
    if (cb < 24UL) return;
    memset(p,0,(size_t)cb);
    d=o2_date_from_filetime(&fd->ftCreationTime,&t);
    put16(p+0,d); put16(p+2,t);
    d=o2_date_from_filetime(&fd->ftLastAccessTime,&t);
    put16(p+4,d); put16(p+6,t);
    d=o2_date_from_filetime(&fd->ftLastWriteTime,&t);
    put16(p+8,d); put16(p+10,t);
    size=fd->nFileSizeLow;
    alloc=(size+4095UL)&~4095UL;
    if (alloc<size) alloc=size;
    attr=o2_attrs_from_win32(fd->dwFileAttributes);
    put32(p+12,size); put32(p+16,alloc); put16(p+20,(WORD)attr);
    n=strlen(fd->cFileName); if (n>255U) n=255U;
    p[22]=(unsigned char)n;
    if (cb>23UL) {
        room=(size_t)(cb-23UL);
        if (n+1U>room) n=room ? room-1U : 0U;
        if (n) memcpy(p+23,fd->cFileName,n);
        if (room) p[23+n]=0;
        p[22]=(unsigned char)n;
    }
}

static void fill_findbuf3(unsigned char *p, O2ULONG cb,
                          const WIN32_FIND_DATAA *fd)
{
    WORD d;
    WORD t;
    DWORD size;
    DWORD alloc;
    O2ULONG attr;
    size_t n;

    /* OS/2 2.x FILEFINDBUF3 begins with oNextEntryOffset.  M28A's
       native-only decoder accidentally omitted this ULONG, so its private
       producer/consumer agreed on a layout that was four bytes too short.
       A real C/386 client using <os2.h> exposed the mismatch immediately. */
    if (cb < 30UL)
        return;
    memset(p, 0, (size_t)cb);

    put32(p + 0, 0UL); /* oNextEntryOffset: one entry per call for now */
    d = o2_date_from_filetime(&fd->ftCreationTime, &t);
    put16(p + 4, d); put16(p + 6, t);
    d = o2_date_from_filetime(&fd->ftLastAccessTime, &t);
    put16(p + 8, d); put16(p + 10, t);
    d = o2_date_from_filetime(&fd->ftLastWriteTime, &t);
    put16(p + 12, d); put16(p + 14, t);

    size = fd->nFileSizeLow;
    alloc = (size + 4095UL) & ~4095UL;
    if (alloc < size)
        alloc = size;
    attr = o2_attrs_from_win32(fd->dwFileAttributes);
    put32(p + 16, size);
    put32(p + 20, alloc);
    put32(p + 24, attr);

    n = strlen(fd->cFileName);
    if (n > 255U)
        n = 255U;
    p[28] = (unsigned char)n;
    if (cb > 29UL) {
        size_t room;
        room = (size_t)(cb - 29UL);
        if (n + 1U > room)
            n = room > 0U ? room - 1U : 0U;
        if (n != 0U)
            memcpy(p + 29, fd->cFileName, n);
        if (room != 0U)
            p[29 + n] = 0;
    }
}

/* 286 - synchronous 32-bit tone generation.
 *
 * OS/2 DosBeep accepts 37..32767 Hz and blocks for the requested number
 * of milliseconds.  WinMM has no direct frequency/duration primitive, so
 * synthesize a mono 16-bit PCM square wave.
 *
 * Milestone 17 deliberately keeps one waveOut device open for the lifetime
 * of the process.  The older implementation opened and closed WAVE_MAPPER for
 * every ~40 ms Sarien note, which inserted audible gaps on some systems.  A
 * CALLBACK_EVENT completion wait also replaces the old Sleep(1) polling loop.
 *
 * The 16.16 phase accumulator is preserved between notes, reducing clicks and
 * keeping this implementation integer-only / ANSI-C89 friendly.
 */
static O2APIRET beep_open_backend(void)
{
    WAVEFORMATEX fmt;
    MMRESULT mm;
    const O2ULONG rate = 44100UL;

    if (beep_wave != NULL)
        return O2_NO_ERROR;

    if (beep_event == NULL) {
        beep_event = CreateEventA(NULL, TRUE, FALSE, NULL);
        if (beep_event == NULL)
            return (O2APIRET)GetLastError();
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = (DWORD)rate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = (DWORD)(rate * 2UL);
    fmt.cbSize = 0;

    ResetEvent(beep_event);
    mm = waveOutOpen(&beep_wave, WAVE_MAPPER, &fmt,
                     (DWORD)beep_event, 0UL, CALLBACK_EVENT);
    if (mm != MMSYSERR_NOERROR) {
        beep_wave = NULL;
        if (audio_trace_enabled()) {
            fprintf(stderr, "DOSCALLS: waveOutOpen failed mm=%u\n", (unsigned)mm);
            fflush(stderr);
        }
        return 1UL;
    }

    /* CALLBACK_EVENT may signal for the open transition.  The next write
       explicitly resets it before submitting audio. */
    ResetEvent(beep_event);
    return O2_NO_ERROR;
}

static O2APIRET beep_reserve_samples(DWORD needed)
{
    short *p;
    DWORD bytes;

    if (needed <= beep_sample_capacity)
        return O2_NO_ERROR;
    if (needed > 0x7fffffffUL / (DWORD)sizeof(short))
        return O2_ERROR_INVALID_PARAMETER;

    bytes = needed * (DWORD)sizeof(short);
    if (beep_samples == NULL)
        p = (short *)HeapAlloc(GetProcessHeap(), 0, bytes);
    else
        p = (short *)HeapReAlloc(GetProcessHeap(), 0, beep_samples, bytes);
    if (p == NULL)
        return (O2APIRET)ERROR_NOT_ENOUGH_MEMORY;

    beep_samples = p;
    beep_sample_capacity = needed;
    return O2_NO_ERROR;
}

/* 284 - DosDevIOCtl.
 *
 * OS/2 exposes device-specific control operations through one nine-argument
 * gateway.  EMX probes this API while classifying inherited descriptors --
 * in particular it uses category 01h ASYNC calls to find out whether an
 * HFILE is a serial device.  A probe which does not apply to the underlying
 * device is a normal error return on OS/2; it must not be treated as a
 * missing-import fatal error by the host.
 *
 * M30J therefore supplies the real 32-bit ABI and deliberately implements a
 * conservative capability boundary.  Unsupported device functions return
 * ERROR_INVALID_CATEGORY/ERROR_INVALID_FUNCTION while leaving caller buffers
 * untouched.  This is sufficient for EMX's normal console/disk HFILE probes
 * and gives us precise category/function traces before we add any device-
 * specific emulation.
 *
 * Category 01h, function 68h (ASYNC_GETINQUECOUNT) gets a small useful
 * implementation for genuine Win32 communications handles.  OS/2 returns a
 * pair of USHORTs: current receive-queue count and queue capacity.  For a
 * normal console, pipe, or disk handle the category is rejected, exactly the
 * path EMX expects when testing whether an HFILE is serial.
 */
O2APIRET __cdecl DosDevIOCtl(O2HFILE hDevice,
                             O2ULONG category,
                             O2ULONG function,
                             void *pParmList,
                             O2ULONG cbParmLengthMax,
                             O2ULONG *pcbParmLengthInOut,
                             void *pDataArea,
                             O2ULONG cbDataLengthMax,
                             O2ULONG *pcbDataLengthInOut)
{
    HANDLE h;
    DCB dcb;
    COMMPROP prop;
    COMSTAT stat;
    DWORD errors;

    (void)pParmList;
    (void)cbParmLengthMax;
    (void)pcbParmLengthInOut;

    if (category > 0xffUL || function > 0xffUL)
        return O2_ERROR_INVALID_PARAMETER;

    /* Some OS/2 IOCTLs use -1 as a pseudo-handle.  No such operation is
     * implemented by M30J yet, but do not accidentally feed it to Win32. */
    if (hDevice == 0xffffffffUL) {
        if (io_trace_enabled()) {
            fprintf(stderr,
                    "M30J DEVIOCTL: hfile=-1 category=%02lX function=%02lX -> unsupported pseudo-handle\n",
                    (unsigned long)category, (unsigned long)function);
            fflush(stderr);
        }
        return O2_ERROR_INVALID_FUNCTION;
    }

    h = os2_handle(hDevice);
    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return O2_ERROR_INVALID_HANDLE;

    if (io_trace_enabled()) {
        fprintf(stderr,
                "M30J DEVIOCTL: hfile=%lu handle=%p native=%s category=%02lX function=%02lX parmMax=%lu parmLen=%s dataMax=%lu dataLen=%s\n",
                (unsigned long)hDevice, (void *)h, win_handle_type_name(h),
                (unsigned long)category, (unsigned long)function,
                (unsigned long)cbParmLengthMax,
                pcbParmLengthInOut != NULL ? "ptr" : "null",
                (unsigned long)cbDataLengthMax,
                pcbDataLengthInOut != NULL ? "ptr" : "null");
        fflush(stderr);
    }

    /* IOCTL_ASYNC / ASYNC_GETINQUECOUNT.  GetCommState is a cheap and
     * reliable discriminator between a Win32 console character handle and a
     * communications handle. */
    if (category == 0x01UL && function == 0x68UL) {
        memset(&dcb, 0, sizeof(dcb));
        dcb.DCBlength = sizeof(dcb);
        if (!GetCommState(h, &dcb))
            return O2_ERROR_INVALID_CATEGORY;

        if (pDataArea == NULL || cbDataLengthMax < 4UL ||
            pcbDataLengthInOut == NULL)
            return O2_ERROR_INVALID_PARAMETER;

        errors = 0;
        memset(&stat, 0, sizeof(stat));
        if (!ClearCommError(h, &errors, &stat))
            return (O2APIRET)GetLastError();

        memset(&prop, 0, sizeof(prop));
        if (!GetCommProperties(h, &prop))
            prop.dwCurrentRxQueue = 0UL;

        ((unsigned short *)pDataArea)[0] =
            (unsigned short)(stat.cbInQue > 0xffffUL ? 0xffffUL : stat.cbInQue);
        ((unsigned short *)pDataArea)[1] =
            (unsigned short)(prop.dwCurrentRxQueue > 0xffffUL ?
                             0xffffUL : prop.dwCurrentRxQueue);
        *pcbDataLengthInOut = 4UL;

        if (io_trace_enabled()) {
            fprintf(stderr,
                    "M30J DEVIOCTL: ASYNC_GETINQUECOUNT count=%u capacity=%u -> rc=0\n",
                    (unsigned int)((unsigned short *)pDataArea)[0],
                    (unsigned int)((unsigned short *)pDataArea)[1]);
            fflush(stderr);
        }
        return O2_NO_ERROR;
    }

    /* The remaining EMX call sites cover optional ASYNC and extension-device
     * operations.  Returning an ordinary OS/2 capability error is preferable
     * to fabricating successful device state; callers can take their normal
     * fallback paths and the trace tells us which operation is actually
     * required next. */
    if (category == 0x01UL)
        return O2_ERROR_INVALID_FUNCTION;
    return O2_ERROR_INVALID_CATEGORY;
}

O2APIRET __cdecl DosBeep(O2ULONG frequency, O2ULONG duration)
{
    WAVEHDR hdr;
    MMRESULT mm;
    O2ULONG sampleCount;
    O2ULONG wholeSeconds;
    O2ULONG remainderMs;
    O2ULONG phase;
    O2ULONG step;
    O2ULONG i;
    O2APIRET rc;
    DWORD bytes;
    DWORD wr;
    const O2ULONG rate = 44100UL;

    if (audio_trace_enabled()) {
        fprintf(stderr, "DOSCALLS: DosBeep frequency=%lu duration=%lu\n",
                (unsigned long)frequency, (unsigned long)duration);
        fflush(stderr);
    }

    /* The historical Sarien beeper uses zero as a rest marker.  It is not a
       legal physical speaker frequency, but treating it as silence is useful
       and harmless for compatibility. */
    if (frequency == 0UL) {
        if (duration != 0UL)
            Sleep((DWORD)duration);
        return O2_NO_ERROR;
    }
    if (frequency < 37UL || frequency > 32767UL)
        return O2_ERROR_INVALID_FREQUENCY;
    if (duration == 0UL)
        return O2_NO_ERROR;

    EnterCriticalSection(&beep_lock);

    rc = beep_open_backend();
    if (rc != O2_NO_ERROR) {
        LeaveCriticalSection(&beep_lock);
        return rc;
    }

    /* Compute rate * duration / 1000 without overflowing on the
       millisecond multiplication first. */
    wholeSeconds = duration / 1000UL;
    remainderMs = duration % 1000UL;
    if (wholeSeconds > (0xffffffffUL / rate)) {
        LeaveCriticalSection(&beep_lock);
        return O2_ERROR_INVALID_PARAMETER;
    }
    sampleCount = wholeSeconds * rate;
    if (sampleCount > 0xffffffffUL - ((remainderMs * rate) / 1000UL)) {
        LeaveCriticalSection(&beep_lock);
        return O2_ERROR_INVALID_PARAMETER;
    }
    sampleCount += (remainderMs * rate) / 1000UL;
    if (sampleCount == 0UL)
        sampleCount = 1UL;

    rc = beep_reserve_samples((DWORD)sampleCount);
    if (rc != O2_NO_ERROR) {
        LeaveCriticalSection(&beep_lock);
        return rc;
    }
    bytes = (DWORD)sampleCount * (DWORD)sizeof(short);

    /* One complete cycle is 0x10000 phase units. */
    step = (frequency * 65536UL) / rate;
    if (step == 0UL)
        step = 1UL;
    phase = beep_phase;
    for (i = 0UL; i < sampleCount; ++i) {
        beep_samples[i] = (short)((phase & 0x8000UL) ? -7000 : 7000);
        phase = (phase + step) & 0xffffUL;
    }
    beep_phase = phase;

    memset(&hdr, 0, sizeof(hdr));
    hdr.lpData = (LPSTR)beep_samples;
    hdr.dwBufferLength = bytes;

    mm = waveOutPrepareHeader(beep_wave, &hdr, sizeof(hdr));
    if (mm != MMSYSERR_NOERROR) {
        if (audio_trace_enabled()) {
            fprintf(stderr, "DOSCALLS: waveOutPrepareHeader failed mm=%u\n", (unsigned)mm);
            fflush(stderr);
        }
        LeaveCriticalSection(&beep_lock);
        return 1UL;
    }

    ResetEvent(beep_event);
    mm = waveOutWrite(beep_wave, &hdr, sizeof(hdr));
    if (mm != MMSYSERR_NOERROR) {
        waveOutUnprepareHeader(beep_wave, &hdr, sizeof(hdr));
        if (audio_trace_enabled()) {
            fprintf(stderr, "DOSCALLS: waveOutWrite failed mm=%u\n", (unsigned)mm);
            fflush(stderr);
        }
        LeaveCriticalSection(&beep_lock);
        return 1UL;
    }

    /* CALLBACK_EVENT can signal on device state changes.  Keep waiting until
       WinMM marks this exact header done; normally this loop runs once. */
    while ((hdr.dwFlags & WHDR_DONE) == 0UL) {
        wr = WaitForSingleObject(beep_event, INFINITE);
        if (wr != WAIT_OBJECT_0) {
            waveOutReset(beep_wave);
            break;
        }
        ResetEvent(beep_event);
    }

    mm = waveOutUnprepareHeader(beep_wave, &hdr, sizeof(hdr));
    if (mm != MMSYSERR_NOERROR && audio_trace_enabled()) {
        fprintf(stderr, "DOSCALLS: waveOutUnprepareHeader failed mm=%u\n", (unsigned)mm);
        fflush(stderr);
    }

    LeaveCriticalSection(&beep_lock);
    return O2_NO_ERROR;
}

/* Compatibility export for the historical 16-bit DOS16BEEP ordinal 50.
 * Translated 32-bit C/386 applications use DosBeep at ordinal 286. */
O2APIRET __cdecl Dos16Beep(O2ULONG frequency, O2ULONG duration)
{
    return DosBeep(frequency, duration);
}

/* 229 */
O2APIRET __cdecl DosSleep(O2ULONG milliseconds)
{
    Sleep((DWORD)milliseconds);
    return O2_NO_ERROR;
}

/* 263 */
O2APIRET __cdecl DosFindClose(O2ULONG hdir)
{
    HANDLE h;
    h = get_find_handle(hdir);
    if (h == INVALID_HANDLE_VALUE)
        return O2_ERROR_INVALID_HANDLE;
    if (!FindClose(h))
        return (O2APIRET)GetLastError();
    free_find_handle(hdir);
    return O2_NO_ERROR;
}

/* 264 */
O2APIRET __cdecl DosFindFirst(const char *filespec, O2ULONG *phdir,
                              O2ULONG attributes, void *findbuf,
                              O2ULONG cbBuf, O2ULONG *pCount,
                              O2ULONG infoLevel)
{
    WIN32_FIND_DATAA fd;
    HANDLE h;
    O2ULONG oh;

    (void)attributes;
    if (!filespec || !phdir || !findbuf || !pCount)
        return O2_ERROR_INVALID_PARAMETER;
    if (infoLevel != O2_FIL_STANDARD)
        return O2_ERROR_INVALID_PARAMETER;

    h = FindFirstFileA(filespec, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        *pCount = 0;
        return (O2APIRET)GetLastError();
    }

    oh = alloc_find_handle(h);
    if (oh == 0xffffffffUL) {
        FindClose(h);
        *pCount = 0;
        return O2_ERROR_TOO_MANY_OPEN_FILES;
    }
    *phdir = oh;
    *pCount = 1;
    if (cbBuf == 279UL)
        fill_findbuf_beta2((unsigned char *)findbuf, cbBuf, &fd);
    else
        fill_findbuf3((unsigned char *)findbuf, cbBuf, &fd);
    return O2_NO_ERROR;
}

/* 265 */
O2APIRET __cdecl DosFindNext(O2ULONG hdir, void *findbuf,
                             O2ULONG cbBuf, O2ULONG *pCount)
{
    WIN32_FIND_DATAA fd;
    HANDLE h;
    DWORD e;
    if (!findbuf || !pCount)
        return O2_ERROR_INVALID_PARAMETER;
    h = get_find_handle(hdir);
    if (h == INVALID_HANDLE_VALUE)
        return O2_ERROR_INVALID_HANDLE;
    if (!FindNextFileA(h, &fd)) {
        e = GetLastError();
        *pCount = 0;
        if (e == ERROR_NO_MORE_FILES)
            return O2_ERROR_NO_MORE_FILES;
        return (O2APIRET)e;
    }
    *pCount = 1;
    if (cbBuf == 279UL)
        fill_findbuf_beta2((unsigned char *)findbuf, cbBuf, &fd);
    else
        fill_findbuf3((unsigned char *)findbuf, cbBuf, &fd);
    return O2_NO_ERROR;
}

/* 311 */
O2APIRET __cdecl DosCreateThread(O2ULONG *ptid, O2THREADFN fn,
                                 O2ULONG param, O2ULONG flags,
                                 O2ULONG stackSize)
{
    struct O2ThreadStart *ts;
    HANDLE h;
    DWORD tid;
    DWORD winflags;

    if (!ptid || !fn)
        return O2_ERROR_INVALID_PARAMETER;

    ts = (struct O2ThreadStart *)HeapAlloc(GetProcessHeap(), 0, sizeof(*ts));
    if (!ts)
        return (O2APIRET)ERROR_NOT_ENOUGH_MEMORY;
    ts->fn = fn;
    ts->arg = param;

    winflags = 0;
    /* The Sarien specimen passes zero; retain a minimal suspended-bit hook. */
    if (flags & 1UL)
        winflags |= CREATE_SUSPENDED;

    h = CreateThread(NULL, (DWORD)stackSize, o2_thread_start, ts,
                     winflags, &tid);
    if (!h) {
        HeapFree(GetProcessHeap(), 0, ts);
        return (O2APIRET)GetLastError();
    }
    *ptid = (O2ULONG)tid;
    CloseHandle(h);
    return O2_NO_ERROR;
}

/*
 * M30 EMX event semaphores (ordinals 324-330).
 *
 * A Win32 manual-reset event provides the blocking primitive, while this
 * small process-local table preserves the OS/2 post count and open/close
 * reference semantics.  EMX depends on the distinction between a successful
 * first post/reset and ERROR_ALREADY_POSTED / ERROR_ALREADY_RESET.
 *
 * Named semaphores are shared between personality callers in this process.
 * Cross-process named semaphore sharing is intentionally deferred; the EMX
 * specimen creates/opens its internal event objects by handle (name == NULL).
 */
#define O2_EVENT_SLOTS 64
#define O2_EVENT_NAME_MAX 127

struct O2EventSem {
    HANDLE native_event;
    O2ULONG generation;
    O2ULONG refs;
    O2ULONG post_count;
    char name[O2_EVENT_NAME_MAX + 1];
};

static struct O2EventSem o2_event_slots[O2_EVENT_SLOTS];
static CRITICAL_SECTION o2_event_lock;
static LONG o2_event_lock_state = 0;

static void o2_event_lock_init(void)
{
    LONG old;
    old = InterlockedCompareExchange(&o2_event_lock_state, 1, 0);
    if (old == 0) {
        InitializeCriticalSection(&o2_event_lock);
        InterlockedExchange(&o2_event_lock_state, 2);
    } else {
        while (InterlockedCompareExchange(&o2_event_lock_state, 2, 2) != 2)
            Sleep(0);
    }
}

static O2ULONG o2_event_make_handle(unsigned int slot, O2ULONG generation)
{
    return (generation << 8) | (O2ULONG)(slot + 1U);
}

static struct O2EventSem *o2_event_from_handle(O2ULONG hev,
                                                unsigned int *slot_out)
{
    unsigned int raw_slot;
    unsigned int slot;
    O2ULONG generation;

    raw_slot = (unsigned int)(hev & 0xffUL);
    if (raw_slot == 0U || raw_slot > O2_EVENT_SLOTS)
        return NULL;
    slot = raw_slot - 1U;
    generation = hev >> 8;
    if (generation == 0UL ||
        o2_event_slots[slot].native_event == NULL ||
        o2_event_slots[slot].generation != generation)
        return NULL;
    if (slot_out)
        *slot_out = slot;
    return &o2_event_slots[slot];
}

/* 324 */
O2APIRET __cdecl DosCreateEventSem(const char *name, O2ULONG *phev,
                                   O2ULONG flags, O2ULONG initialState)
{
    unsigned int i;
    int free_slot;
    struct O2EventSem *sem;
    HANDLE h;
    size_t n;

    (void)flags;
    if (!phev)
        return O2_ERROR_INVALID_PARAMETER;
    if (name) {
        n = strlen(name);
        if (n > O2_EVENT_NAME_MAX)
            return O2_ERROR_BUFFER_OVERFLOW;
    }

    o2_event_lock_init();
    EnterCriticalSection(&o2_event_lock);

    free_slot = -1;
    for (i = 0; i < O2_EVENT_SLOTS; ++i) {
        sem = &o2_event_slots[i];
        if (sem->native_event == NULL) {
            if (free_slot < 0)
                free_slot = (int)i;
            continue;
        }
        if (name && sem->name[0] != '\0' && strcmp(sem->name, name) == 0) {
            LeaveCriticalSection(&o2_event_lock);
            return O2_ERROR_DUPLICATE_NAME;
        }
    }

    if (free_slot < 0) {
        LeaveCriticalSection(&o2_event_lock);
        return O2_ERROR_TOO_MANY_OPENS;
    }

    h = CreateEventA(NULL, TRUE, initialState ? TRUE : FALSE, NULL);
    if (!h) {
        O2APIRET rc;
        rc = (O2APIRET)GetLastError();
        LeaveCriticalSection(&o2_event_lock);
        return rc;
    }

    sem = &o2_event_slots[(unsigned int)free_slot];
    sem->generation++;
    if (sem->generation == 0UL)
        sem->generation = 1UL;
    sem->native_event = h;
    sem->refs = 1UL;
    sem->post_count = initialState ? 1UL : 0UL;
    sem->name[0] = '\0';
    if (name)
        strcpy(sem->name, name);
    *phev = o2_event_make_handle((unsigned int)free_slot, sem->generation);

    LeaveCriticalSection(&o2_event_lock);
    return O2_NO_ERROR;
}

/* 325 */
O2APIRET __cdecl DosOpenEventSem(const char *name, O2ULONG *phev)
{
    unsigned int i;
    struct O2EventSem *sem;

    if (!phev)
        return O2_ERROR_INVALID_PARAMETER;

    o2_event_lock_init();
    EnterCriticalSection(&o2_event_lock);

    if (name) {
        for (i = 0; i < O2_EVENT_SLOTS; ++i) {
            sem = &o2_event_slots[i];
            if (sem->native_event != NULL && sem->name[0] != '\0' &&
                strcmp(sem->name, name) == 0) {
                sem->refs++;
                *phev = o2_event_make_handle(i, sem->generation);
                LeaveCriticalSection(&o2_event_lock);
                return O2_NO_ERROR;
            }
        }
        LeaveCriticalSection(&o2_event_lock);
        return O2_ERROR_INVALID_HANDLE;
    }

    sem = o2_event_from_handle(*phev, NULL);
    if (!sem) {
        LeaveCriticalSection(&o2_event_lock);
        return O2_ERROR_INVALID_HANDLE;
    }
    sem->refs++;
    LeaveCriticalSection(&o2_event_lock);
    return O2_NO_ERROR;
}

/* 326 */
O2APIRET __cdecl DosCloseEventSem(O2ULONG hev)
{
    struct O2EventSem *sem;
    HANDLE h;

    o2_event_lock_init();
    EnterCriticalSection(&o2_event_lock);
    sem = o2_event_from_handle(hev, NULL);
    if (!sem) {
        LeaveCriticalSection(&o2_event_lock);
        return O2_ERROR_INVALID_HANDLE;
    }

    if (--sem->refs != 0UL) {
        LeaveCriticalSection(&o2_event_lock);
        return O2_NO_ERROR;
    }

    h = sem->native_event;
    sem->native_event = NULL;
    sem->post_count = 0UL;
    sem->name[0] = '\0';
    LeaveCriticalSection(&o2_event_lock);

    if (!CloseHandle(h))
        return (O2APIRET)GetLastError();
    return O2_NO_ERROR;
}

/* 327 */
O2APIRET __cdecl DosResetEventSem(O2ULONG hev, O2ULONG *postCount)
{
    struct O2EventSem *sem;
    O2ULONG old_count;

    if (!postCount)
        return O2_ERROR_INVALID_PARAMETER;

    o2_event_lock_init();
    EnterCriticalSection(&o2_event_lock);
    sem = o2_event_from_handle(hev, NULL);
    if (!sem) {
        LeaveCriticalSection(&o2_event_lock);
        return O2_ERROR_INVALID_HANDLE;
    }

    old_count = sem->post_count;
    *postCount = old_count;
    if (old_count == 0UL) {
        LeaveCriticalSection(&o2_event_lock);
        return O2_ERROR_ALREADY_RESET;
    }
    sem->post_count = 0UL;
    if (!ResetEvent(sem->native_event)) {
        O2APIRET rc;
        rc = (O2APIRET)GetLastError();
        LeaveCriticalSection(&o2_event_lock);
        return rc;
    }
    LeaveCriticalSection(&o2_event_lock);
    return O2_NO_ERROR;
}

/* 328 */
O2APIRET __cdecl DosPostEventSem(O2ULONG hev)
{
    struct O2EventSem *sem;
    int already_posted;

    o2_event_lock_init();
    EnterCriticalSection(&o2_event_lock);
    sem = o2_event_from_handle(hev, NULL);
    if (!sem) {
        LeaveCriticalSection(&o2_event_lock);
        return O2_ERROR_INVALID_HANDLE;
    }

    already_posted = (sem->post_count != 0UL);
    sem->post_count++;
    if (!SetEvent(sem->native_event)) {
        O2APIRET rc;
        sem->post_count--;
        rc = (O2APIRET)GetLastError();
        LeaveCriticalSection(&o2_event_lock);
        return rc;
    }
    LeaveCriticalSection(&o2_event_lock);
    return already_posted ? O2_ERROR_ALREADY_POSTED : O2_NO_ERROR;
}

/* 329 */
O2APIRET __cdecl DosWaitEventSem(O2ULONG hev, O2ULONG timeout)
{
    struct O2EventSem *sem;
    HANDLE h;
    DWORD rc;
    DWORD ms;

    o2_event_lock_init();
    EnterCriticalSection(&o2_event_lock);
    sem = o2_event_from_handle(hev, NULL);
    if (!sem) {
        LeaveCriticalSection(&o2_event_lock);
        return O2_ERROR_INVALID_HANDLE;
    }
    h = sem->native_event;
    LeaveCriticalSection(&o2_event_lock);

    ms = (timeout == O2_SEM_INDEFINITE_WAIT) ? INFINITE : (DWORD)timeout;
    rc = WaitForSingleObject(h, ms);
    if (rc == WAIT_OBJECT_0)
        return O2_NO_ERROR;
    if (rc == WAIT_TIMEOUT)
        return (O2APIRET)ERROR_SEM_TIMEOUT;
    return (O2APIRET)GetLastError();
}

/* 330 */
O2APIRET __cdecl DosQueryEventSem(O2ULONG hev, O2ULONG *postCount)
{
    struct O2EventSem *sem;

    if (!postCount)
        return O2_ERROR_INVALID_PARAMETER;
    o2_event_lock_init();
    EnterCriticalSection(&o2_event_lock);
    sem = o2_event_from_handle(hev, NULL);
    if (!sem) {
        LeaveCriticalSection(&o2_event_lock);
        return O2_ERROR_INVALID_HANDLE;
    }
    *postCount = sem->post_count;
    LeaveCriticalSection(&o2_event_lock);
    return O2_NO_ERROR;
}

static void o2_mutex_native_name(const char *name, char *out)
{
    O2ULONG hash;
    static const char hex[] = "0123456789ABCDEF";
    unsigned int i;
    const unsigned char *p;
    const char prefix[] = "Local\\OS2HOST32_MTX_";
    size_t prefix_len;

    hash = 2166136261UL;
    p = (const unsigned char *)name;
    while (*p) {
        hash ^= (O2ULONG)*p++;
        hash *= 16777619UL;
    }

    prefix_len = strlen(prefix);
    memcpy(out, prefix, prefix_len);
    for (i = 0; i < 8U; ++i) {
        unsigned int shift;
        shift = (7U - i) * 4U;
        out[prefix_len + i] = hex[(unsigned int)((hash >> shift) & 0x0fUL)];
    }
    out[prefix_len + 8U] = '\0';
}

/* 331 */
O2APIRET __cdecl DosCreateMutexSem(const char *name, O2ULONG *phmtx,
                                   O2ULONG flags, O2ULONG initialOwner)
{
    HANDLE h;
    char native_name[64];
    const char *native_name_ptr;
    DWORD create_error;

    (void)flags;
    if (!phmtx)
        return O2_ERROR_INVALID_PARAMETER;

    native_name_ptr = NULL;
    if (name) {
        o2_mutex_native_name(name, native_name);
        native_name_ptr = native_name;
    }

    SetLastError(ERROR_SUCCESS);
    h = CreateMutexA(NULL, initialOwner ? TRUE : FALSE, native_name_ptr);
    if (!h)
        return (O2APIRET)GetLastError();
    create_error = GetLastError();
    if (name && create_error == ERROR_ALREADY_EXISTS) {
        CloseHandle(h);
        return O2_ERROR_DUPLICATE_NAME;
    }

    *phmtx = (O2ULONG)(DWORD)h;
    return O2_NO_ERROR;
}

/* 332 */
O2APIRET __cdecl DosOpenMutexSem(const char *name, O2ULONG *phmtx)
{
    HANDLE h;
    char native_name[64];
    DWORD e;

    if (!name || !phmtx)
        return O2_ERROR_INVALID_PARAMETER;
    o2_mutex_native_name(name, native_name);
    h = OpenMutexA(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, native_name);
    if (!h) {
        e = GetLastError();
        if (e == ERROR_FILE_NOT_FOUND)
            return O2_ERROR_SEM_NOT_FOUND;
        return (O2APIRET)e;
    }
    *phmtx = (O2ULONG)(DWORD)h;
    return O2_NO_ERROR;
}

/* 333 */
O2APIRET __cdecl DosCloseMutexSem(O2ULONG hmtx)
{
    if (!hmtx)
        return O2_ERROR_INVALID_HANDLE;
    if (!CloseHandle((HANDLE)(DWORD)hmtx))
        return (O2APIRET)GetLastError();
    return O2_NO_ERROR;
}

/* 334 */
O2APIRET __cdecl DosRequestMutexSem(O2ULONG hmtx, O2ULONG timeout)
{
    DWORD rc;
    DWORD ms;
    ms = (timeout == O2_SEM_INDEFINITE_WAIT) ? INFINITE : (DWORD)timeout;
    rc = WaitForSingleObject((HANDLE)(DWORD)hmtx, ms);
    if (rc == WAIT_OBJECT_0 || rc == WAIT_ABANDONED)
        return O2_NO_ERROR;
    if (rc == WAIT_TIMEOUT)
        return (O2APIRET)ERROR_SEM_TIMEOUT;
    return (O2APIRET)GetLastError();
}

/* 335 */
O2APIRET __cdecl DosReleaseMutexSem(O2ULONG hmtx)
{
    if (!ReleaseMutex((HANDLE)(DWORD)hmtx))
        return (O2APIRET)GetLastError();
    return O2_NO_ERROR;
}

/* 230 - DATETIME layout used by the 32-bit API:
 *   +0 hours       UCHAR
 *   +1 minutes     UCHAR
 *   +2 seconds     UCHAR
 *   +3 hundredths  UCHAR
 *   +4 day         UCHAR
 *   +5 month       UCHAR
 *   +6 year        USHORT
 *   +8 timezone    SHORT (minutes west of UTC)
 *  +10 dayofweek   UCHAR (0 = Sunday)
 *  +11 padding
 */
O2APIRET __cdecl DosGetDateTime(void *buffer)
{
    return (O2APIRET)os2_core_DosGetDateTime(
        &o2_personality_context, o2_pointer_address(buffer));
}

/* 220 */
O2APIRET __cdecl DosSetDefaultDisk(O2ULONG diskNum)
{
    char spec[4];
    char full[MAX_PATH];
    DWORD n;

    if (diskNum < 1UL || diskNum > 26UL)
        return O2_ERROR_INVALID_PARAMETER;
    spec[0] = (char)('A' + (int)diskNum - 1);
    spec[1] = ':';
    spec[2] = '.';
    spec[3] = '\0';
    n = GetFullPathNameA(spec, (DWORD)sizeof(full), full, NULL);
    if (n == 0 || n >= (DWORD)sizeof(full))
        return (O2APIRET)GetLastError();
    if (!SetCurrentDirectoryA(full))
        return (O2APIRET)GetLastError();
    return O2_NO_ERROR;
}

/* 227 */
O2APIRET __cdecl DosScanEnv(const char *name, char **value)
{
    DWORD n;

    if (!name || !*name || !value)
        return O2_ERROR_INVALID_PARAMETER;
    n = GetEnvironmentVariableA(name, o2_scan_env,
                                (DWORD)sizeof(o2_scan_env));
    if (n == 0) {
        *value = NULL;
        return O2_ERROR_ENVVAR_NOT_FOUND;
    }
    if (n >= (DWORD)sizeof(o2_scan_env)) {
        *value = NULL;
        return O2_ERROR_BUFFER_OVERFLOW;
    }
    *value = o2_scan_env;
    return O2_NO_ERROR;
}

/* 228 */
O2APIRET __cdecl DosSearchPath(O2ULONG flags, const char *pathOrName,
                               const char *filename, char *buffer,
                               O2ULONG cbBuffer)
{
    char paths[4096];
    char envbuf[4096];
    char part[MAX_PATH];
    char candidate[MAX_PATH * 2];
    char full[MAX_PATH * 2];
    const char *src;
    const char *q;
    const char *semi;
    size_t n;
    DWORD got;
    DWORD attr;
    int wildcard;

    if (!filename || !buffer || cbBuffer == 0UL)
        return O2_ERROR_INVALID_PARAMETER;
    paths[0]=0;

    if ((flags & 0x0001UL) != 0UL) {
        strcpy(paths,".");
        if (pathOrName && *pathOrName) strcat(paths,";");
    }
    if ((flags & 0x0002UL) != 0UL) {
        if (!pathOrName || !*pathOrName)
            return 203UL; /* ERROR_ENVVAR_NOT_FOUND */
        got=GetEnvironmentVariableA(pathOrName,envbuf,(DWORD)sizeof(envbuf));
        if (got == 0 || got >= (DWORD)sizeof(envbuf))
            return got == 0 ? 203UL : O2_ERROR_BUFFER_OVERFLOW;
        if (paths[0]) strcat(paths,";");
        strcat(paths,envbuf);
    } else if (pathOrName && *pathOrName) {
        if (paths[0]) strcat(paths,";");
        if (strlen(paths)+strlen(pathOrName)+1U >= sizeof(paths))
            return O2_ERROR_BUFFER_OVERFLOW;
        strcat(paths,pathOrName);
    }
    if (!paths[0]) strcpy(paths,".");

    wildcard = strchr(filename,'*') != NULL || strchr(filename,'?') != NULL;
    src=paths;
    while (*src) {
        semi=strchr(src,';');
        q=semi ? semi : src+strlen(src);
        n=(size_t)(q-src);
        if (n >= sizeof(part)) n=sizeof(part)-1U;
        memcpy(part,src,n); part[n]=0;
        if (!part[0]) strcpy(part,".");
        n=strlen(part);
        if (n && part[n-1] != '\\' && part[n-1] != '/')
            sprintf(candidate,"%s\\%s",part,filename);
        else
            sprintf(candidate,"%s%s",part,filename);
        got=GetFullPathNameA(candidate,(DWORD)sizeof(full),full,NULL);
        if (got != 0 && got < (DWORD)sizeof(full)) {
            if (wildcard) {
                char parent[MAX_PATH * 2];
                char *slash;
                strcpy(parent,full);
                slash=strrchr(parent,'\\');
                if (!slash) slash=strrchr(parent,'/');
                if (slash) *slash=0;
                attr=GetFileAttributesA(parent[0] ? parent : ".");
                if (attr != INVALID_FILE_ATTRIBUTES &&
                    (attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
                    goto found;
            } else {
                attr=GetFileAttributesA(full);
                if (attr != INVALID_FILE_ATTRIBUTES)
                    goto found;
            }
        }
        if (!semi) break;
        src=semi+1;
    }
    buffer[0]=0;
    return 2UL; /* ERROR_FILE_NOT_FOUND */
found:
    if (strlen(full)+1U > (size_t)cbBuffer) {
        buffer[0]=0;
        return O2_ERROR_BUFFER_OVERFLOW;
    }
    strcpy(buffer,full);
    return O2_NO_ERROR;
}

/* 312 */
O2APIRET __cdecl DosGetInfoBlocks(void **pptib, void **pppib)
{
    LPCH env;
    LPCH p;
    size_t bytes;
    const char *cmd;
    char *guest_cmd;
    O2ULONG guest_cmd_len;
    size_t cmdlen;

    if (!pptib || !pppib)
        return O2_ERROR_INVALID_PARAMETER;

    env = GetEnvironmentStringsA();
    if (!env)
        return (O2APIRET)GetLastError();
    p = env;
    while (*p != '\0')
        p += strlen(p) + 1U;
    ++p;
    bytes = (size_t)(p - env);
    if (bytes > sizeof(o2_info_env)) {
        FreeEnvironmentStringsA(env);
        return O2_ERROR_BUFFER_OVERFLOW;
    }
    memcpy(o2_info_env, env, bytes);
    FreeEnvironmentStringsA(env);

    guest_cmd = NULL;
    guest_cmd_len = 0;
    if (o2_query_main_guest_command(&guest_cmd, &guest_cmd_len) &&
        guest_cmd_len <= (O2ULONG)sizeof(o2_info_cmd)) {
        memcpy(o2_info_cmd, guest_cmd, (size_t)guest_cmd_len);
        cmdlen = (size_t)guest_cmd_len;
    } else {
        /* Compatibility fallback for DOSCALLS used outside OS2HOST32. */
        cmd = GetCommandLineA();
        if (!cmd)
            cmd = "";
        cmdlen = strlen(cmd) + 1U;
        if (cmdlen + 2U > sizeof(o2_info_cmd))
            cmdlen = sizeof(o2_info_cmd) - 2U;
        memcpy(o2_info_cmd, cmd, cmdlen);
        o2_info_cmd[cmdlen++] = '\0';
        o2_info_cmd[cmdlen++] = '\0';
    }

    memset(&o2_info_tib, 0, sizeof(o2_info_tib));
    memset(&o2_info_tib2, 0, sizeof(o2_info_tib2));
    memset(&o2_info_pib, 0, sizeof(o2_info_pib));
    o2_info_tib.pexchain = o2_exception_head;

    /* M30D: OS/2 TIB offsets +4/+8 describe the current thread's stack
     * bounds.  EMX checks these during _emx_init and rejects a span of
     * 16K or less.  Ask the in-process loader for the actual mapped guest
     * stack rather than exposing the Win32 host thread's stack. */
    {
        O2ULONG stack_low = 0;
        O2ULONG stack_high = 0;
        if (o2_query_main_guest_stack(&stack_low, &stack_high) &&
            stack_high > stack_low) {
            o2_info_tib.pstack = (void *)(ULONG_PTR)stack_low;
            o2_info_tib.pstacklimit = (void *)(ULONG_PTR)stack_high;
        }
    }
    o2_info_tib2.tid = 1UL;
    o2_info_tib.ptib2 = &o2_info_tib2;
    o2_info_tib.ordinal = 1UL;

    o2_info_pib.pid = (O2ULONG)GetCurrentProcessId();
    /* M30: OS2HOST32 reserves HMODULE/HMTE 1 for the main guest image. */
    o2_info_pib.hmte = 1UL;
    o2_info_pib.pchcmd = o2_info_cmd;
    o2_info_pib.pchenv = o2_info_env;
    if (GetEnvironmentVariableA("OS2_TRACE_EMX_SELF", NULL, 0) != 0) {
        const char *tail;
        tail = o2_info_cmd + strlen(o2_info_cmd) + 1U;
        fprintf(stderr,
                "M30M2 PIB CMD: argv0=\"%s\" tail=\"%s\" bytes=%lu\n",
                o2_info_cmd, tail, (unsigned long)cmdlen);
        fflush(stderr);
    }
    o2_info_pib.ultype = 0UL; /* fullscreen/console-compatible default */

    *pptib = &o2_info_tib;
    *pppib = &o2_info_pib;
    if (GetEnvironmentVariableA("OS2_TRACE_EMX_SELF", NULL, 0) != 0) {
        fprintf(stderr,
                "M30D TIB: pstack=%08lX limit=%08lX span=%lu KB tid=%lu\n",
                (unsigned long)(ULONG_PTR)o2_info_tib.pstack,
                (unsigned long)(ULONG_PTR)o2_info_tib.pstacklimit,
                (unsigned long)(((ULONG_PTR)o2_info_tib.pstacklimit >
                                 (ULONG_PTR)o2_info_tib.pstack) ?
                                (((ULONG_PTR)o2_info_tib.pstacklimit -
                                  (ULONG_PTR)o2_info_tib.pstack) / 1024UL) : 0UL),
                (unsigned long)o2_info_tib2.tid);
    }
    return O2_NO_ERROR;
}

/* 212 - DosError.  OS/2 uses this to select process error-notification
   policy.  There is no direct Win32 equivalent needed by the compatibility
   runtime, but preserving and validating the process-local flag state gives
   callers the documented API contract. */
static O2ULONG g_dos_error_flags = 1UL;

O2APIRET __cdecl DosError(O2ULONG flags)
{
    if (flags & ~3UL)
        return O2_ERROR_INVALID_PARAMETER;
    g_dos_error_flags = flags;
    return O2_NO_ERROR;
}

/* 218 - set standard metadata on an open OS/2 HFILE. */
O2APIRET __cdecl DosSetFileInfo(O2HFILE hFile, O2ULONG level,
                                const void *buffer, O2ULONG cb)
{
    const unsigned char *p;
    HANDLE h;
    FILETIME creation;
    FILETIME access;
    FILETIME write;
    DWORD wanted;
    DWORD err;
    WORD cd, ct, ad, at, wd, wt;

    if (!buffer)
        return O2_ERROR_INVALID_PARAMETER;
    if (level != O2_FIL_STANDARD || cb < 24UL)
        return O2_ERROR_INVALID_PARAMETER;
    h = os2_handle(hFile);
    if (h == INVALID_HANDLE_VALUE)
        return O2_ERROR_INVALID_HANDLE;

    p = (const unsigned char *)buffer;
    cd = get16(p + 0);  ct = get16(p + 2);
    ad = get16(p + 4);  at = get16(p + 6);
    wd = get16(p + 8);  wt = get16(p + 10);
    if ((cd || ct) && !o2_filetime_from_date_time(cd, ct, &creation))
        return O2_ERROR_INVALID_PARAMETER;
    if ((ad || at) && !o2_filetime_from_date_time(ad, at, &access))
        return O2_ERROR_INVALID_PARAMETER;
    if ((wd || wt) && !o2_filetime_from_date_time(wd, wt, &write))
        return O2_ERROR_INVALID_PARAMETER;

    if (!SetFileTime(h, (cd || ct) ? &creation : NULL,
                     (ad || at) ? &access : NULL,
                     (wd || wt) ? &write : NULL))
        return (O2APIRET)GetLastError();

    /* FILESTATUS3's file-size/allocation fields are query-only.  Attributes
       are writable, but legacy Win32 has only pathname-based attribute APIs.
       Resolve the open handle's DOS pathname dynamically on NT 6+ so the
       compatibility DLL keeps old-header buildability while doing the real
       update on the Windows versions supported by os2host32. */
    wanted = get32(p + 20);
    {
        typedef DWORD (WINAPI *PFNGETFINALPATHNAMEBYHANDLEA)(HANDLE, LPSTR,
                                                              DWORD, DWORD);
        PFNGETFINALPATHNAMEBYHANDLEA get_final;
        HMODULE kernel;
        char raw[MAX_PATH * 2];
        char path[MAX_PATH * 2];
        DWORD n;
        DWORD attrs;

        kernel = GetModuleHandleA("kernel32.dll");
        get_final = kernel ? (PFNGETFINALPATHNAMEBYHANDLEA)
                    GetProcAddress(kernel, "GetFinalPathNameByHandleA") : NULL;
        if (!get_final)
            return O2_ERROR_INVALID_FUNCTION;
        n = get_final(h, raw, (DWORD)sizeof(raw), 0);
        if (n == 0)
            return (O2APIRET)GetLastError();
        if (n >= (DWORD)sizeof(raw))
            return O2_ERROR_BUFFER_OVERFLOW;
        if (strncmp(raw, "\\\\?\\UNC\\", 8) == 0) {
            strcpy(path, "\\\\");
            strncat(path, raw + 8, sizeof(path) - 3);
            path[sizeof(path) - 1] = 0;
        } else if (strncmp(raw, "\\\\?\\", 4) == 0) {
            strncpy(path, raw + 4, sizeof(path) - 1);
            path[sizeof(path) - 1] = 0;
        } else {
            strncpy(path, raw, sizeof(path) - 1);
            path[sizeof(path) - 1] = 0;
        }

        attrs = GetFileAttributesA(path);
        if (attrs == INVALID_FILE_ATTRIBUTES)
            return (O2APIRET)GetLastError();
        attrs &= ~(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN |
                   FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE |
                   FILE_ATTRIBUTE_NORMAL);
        if (wanted & O2_FILE_READONLY) attrs |= FILE_ATTRIBUTE_READONLY;
        if (wanted & O2_FILE_HIDDEN)   attrs |= FILE_ATTRIBUTE_HIDDEN;
        if (wanted & O2_FILE_SYSTEM)   attrs |= FILE_ATTRIBUTE_SYSTEM;
        if (wanted & O2_FILE_ARCHIVED) attrs |= FILE_ATTRIBUTE_ARCHIVE;
        if (attrs == 0)
            attrs = FILE_ATTRIBUTE_NORMAL;
        if (!SetFileAttributesA(path, attrs)) {
            err = GetLastError();
            return (O2APIRET)err;
        }
    }
    return O2_NO_ERROR;
}

/* 219 - set standard pathname metadata. */
O2APIRET __cdecl DosSetPathInfo(const char *path, O2ULONG level,
                                const void *buffer, O2ULONG cb,
                                O2ULONG options)
{
    const unsigned char *p;
    FILETIME creation;
    FILETIME access;
    FILETIME write;
    HANDLE h;
    DWORD attrs;
    DWORD wanted;
    DWORD err;

    (void)options;
    if (!path || !buffer)
        return O2_ERROR_INVALID_PARAMETER;
    if (level != O2_FIL_STANDARD || cb < 24UL)
        return O2_ERROR_INVALID_PARAMETER;

    p = (const unsigned char *)buffer;
    {
        WORD cd, ct, ad, at, wd, wt;
        cd = get16(p + 0);  ct = get16(p + 2);
        ad = get16(p + 4);  at = get16(p + 6);
        wd = get16(p + 8);  wt = get16(p + 10);
        if ((cd || ct) && !o2_filetime_from_date_time(cd, ct, &creation))
            return O2_ERROR_INVALID_PARAMETER;
        if ((ad || at) && !o2_filetime_from_date_time(ad, at, &access))
            return O2_ERROR_INVALID_PARAMETER;
        if ((wd || wt) && !o2_filetime_from_date_time(wd, wt, &write))
            return O2_ERROR_INVALID_PARAMETER;

        /* OS/2 leaves a timestamp unchanged when both of its packed date/time
           fields are zero. */
        h = CreateFileA(path, FILE_WRITE_ATTRIBUTES,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (h == INVALID_HANDLE_VALUE)
            return (O2APIRET)GetLastError();
        if (!SetFileTime(h, (cd || ct) ? &creation : NULL,
                         (ad || at) ? &access : NULL,
                         (wd || wt) ? &write : NULL)) {
            err = GetLastError();
            CloseHandle(h);
            return (O2APIRET)err;
        }
        CloseHandle(h);
    }

    /* cbFile/cbFileAlloc are query fields for FIL_STANDARD.  Attribute and
       timestamp updates are the writable portion used by OS/2 callers. */
    wanted = get32(p + 20);
    attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return (O2APIRET)GetLastError();
    attrs &= ~(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN |
               FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE |
               FILE_ATTRIBUTE_NORMAL);
    if (wanted & O2_FILE_READONLY) attrs |= FILE_ATTRIBUTE_READONLY;
    if (wanted & O2_FILE_HIDDEN)   attrs |= FILE_ATTRIBUTE_HIDDEN;
    if (wanted & O2_FILE_SYSTEM)   attrs |= FILE_ATTRIBUTE_SYSTEM;
    if (wanted & O2_FILE_ARCHIVED) attrs |= FILE_ATTRIBUTE_ARCHIVE;
    if (attrs == 0)
        attrs = FILE_ATTRIBUTE_NORMAL;
    if (!SetFileAttributesA(path, attrs))
        return (O2APIRET)GetLastError();
    return O2_NO_ERROR;
}

/* 163 - query executable application type.  The OS/2 LE/LX module flags
 * encode PM compatibility in bits 8-9: 1=NOTWINDOWCOMPAT, 2=WINDOWCOMPAT,
 * 3=WINDOWAPI.  CMD32 only needs those historical low application-type bits,
 * but keeping the decode here restores the proper DOSCALLS loader boundary. */
O2APIRET __cdecl DosQAppType(const char *path, O2ULONG *appType)
{
    unsigned char mz[64];
    unsigned char hdr[24];
    HANDLE h;
    DWORD got;
    DWORD off;
    DWORD flags;
    DWORD err;

    if (!path || !appType)
        return O2_ERROR_INVALID_PARAMETER;
    *appType = 0UL;

    h = CreateFileA(path, GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return (O2APIRET)GetLastError();

    got = 0;
    if (!ReadFile(h, mz, (DWORD)sizeof(mz), &got, NULL)) {
        err = GetLastError();
        CloseHandle(h);
        return (O2APIRET)err;
    }
    if (got < (DWORD)sizeof(mz)) {
        CloseHandle(h);
        return O2_ERROR_INVALID_EXE_SIGNATURE;
    }
    if (mz[0] != 'M' || mz[1] != 'Z') {
        CloseHandle(h);
        return O2_ERROR_INVALID_EXE_SIGNATURE;
    }

    off = (DWORD)get32(mz + 0x3c);
    if (SetFilePointer(h, (LONG)off, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER &&
        GetLastError() != NO_ERROR) {
        err = GetLastError();
        CloseHandle(h);
        return (O2APIRET)err;
    }
    got = 0;
    if (!ReadFile(h, hdr, (DWORD)sizeof(hdr), &got, NULL)) {
        err = GetLastError();
        CloseHandle(h);
        return (O2APIRET)err;
    }
    if (got < (DWORD)sizeof(hdr)) {
        CloseHandle(h);
        return O2_ERROR_INVALID_EXE_SIGNATURE;
    }
    CloseHandle(h);

    if (!((hdr[0] == 'L' && hdr[1] == 'E') ||
          (hdr[0] == 'L' && hdr[1] == 'X')))
        return O2_ERROR_INVALID_EXE_SIGNATURE;

    flags = (DWORD)get32(hdr + 0x10);
    *appType = (O2ULONG)((flags >> 8) & 0x03UL);
    return O2_NO_ERROR;
}

/* 323 - 32-bit OS/2 2.x DosQueryAppType.  The prerelease C/386 headers used
 * by CMD32 still spell the source-level call DosQAppType, but OS2386.LIB
 * resolves that 32-bit API to DOSCALLS.323.  Keep ordinal 163 as the older
 * compatibility name and publish the same classifier at the proper 32-bit
 * entry point as well. */
O2APIRET __cdecl DosQueryAppType(const char *path, O2ULONG *appType)
{
    return DosQAppType(path, appType);
}


/* 223 */
O2APIRET __cdecl DosQueryPathInfo(const char *path, O2ULONG level,
                                  void *buffer, O2ULONG cb)
{
    WIN32_FIND_DATAA fd;
    HANDLE findh;

    if (!path || !buffer)
        return O2_ERROR_INVALID_PARAMETER;

    if (level == O2_FIL_STANDARD) {
        if (cb < 24UL)
            return O2_ERROR_INVALID_PARAMETER;
        findh = FindFirstFileA(path, &fd);
        if (findh == INVALID_HANDLE_VALUE)
            return (O2APIRET)GetLastError();
        FindClose(findh);
        fill_fil_standard((unsigned char *)buffer, &fd);
        return O2_NO_ERROR;
    }

    if (level == O2_FIL_QUERYFULLNAME) {
        DWORD n;
        n = GetFullPathNameA(path, (DWORD)cb, (char *)buffer, NULL);
        if (n == 0)
            return (O2APIRET)GetLastError();
        if (n >= (DWORD)cb)
            return O2_ERROR_INVALID_PARAMETER;
        return O2_NO_ERROR;
    }

    return O2_ERROR_INVALID_PARAMETER;
}

/* 224 */
O2APIRET __cdecl DosQueryHType(O2HFILE hFile, O2ULONG *pType, O2ULONG *pAttr)
{
    return (O2APIRET)os2_core_DosQueryHType(
        &o2_personality_context, (os2_handle32_t)hFile,
        o2_pointer_address(pType), o2_pointer_address(pAttr));
}

/* 296 - process exit-list registration.
 *
 * M30 only needs the contract used by EMX startup.  Keep the registration
 * deliberately non-executing for the first bridge: invoking guest cleanup
 * callbacks from inside the Win32 termination path would expand the supported
 * surface before the program itself has proved useful.  ADD/REMOVE/EXIT are
 * nevertheless accepted with their documented low-byte function codes. */
O2APIRET __cdecl DosExitList(O2ULONG orderCode, void (__cdecl *routine)(O2ULONG))
{
    O2ULONG fn;
    fn = orderCode & 0xffUL;
    if (fn < 1UL || fn > 3UL)
        return O2_ERROR_INVALID_FUNCTION;
    if ((fn == 1UL || fn == 2UL) && routine == NULL)
        return O2_ERROR_INVALID_PARAMETER;
    return O2_NO_ERROR;
}

/* 232 */
O2APIRET __cdecl DosEnterCritSec(void)
{
    /* WinPostMsg is asynchronous; HANOI uses the process critical section
     * only to keep its completion post from being consumed before the worker
     * has terminated.  The host message queue already preserves that useful
     * ordering for this path.  A scheduler-wide critical section can be
     * added when a specimen actually requires it. */
    return O2_NO_ERROR;
}

/* 234 */
void __cdecl DosExit(O2ULONG action, O2ULONG result)
{
    if (action == 0UL) { /* EXIT_THREAD */
        ExitThread((DWORD)result);
        return;
    }
    o2_module_process_exit();
    ExitProcess((UINT)result);
}

/* 226 */
O2APIRET __cdecl DosDeleteDir(const char *path)
{
    if (!path)
        return O2_ERROR_INVALID_PARAMETER;
    if (!RemoveDirectoryA(path))
        return (O2APIRET)GetLastError();
    return O2_NO_ERROR;
}

/* 255 */
O2APIRET __cdecl DosSetCurrentDir(const char *path)
{
    char oldcwd[MAX_PATH];
    char restore[3];
    DWORD n;
    char olddrive;
    char targetdrive;

    if (!path)
        return O2_ERROR_INVALID_PARAMETER;

    /* OS/2 keeps a current directory for every drive.  A drive-qualified
     * DosSetCurrentDir updates that drive's CDS entry without selecting it as
     * the default disk.  Win32 has compatible per-drive bookkeeping, but
     * SetCurrentDirectory also switches the process drive; switch back after
     * updating a non-default drive. */
    n = GetCurrentDirectoryA((DWORD)sizeof(oldcwd), oldcwd);
    if (n == 0 || n >= (DWORD)sizeof(oldcwd))
        return (O2APIRET)GetLastError();
    olddrive = oldcwd[0];
    if (olddrive >= 'a' && olddrive <= 'z')
        olddrive = (char)(olddrive - 'a' + 'A');
    targetdrive = path[0];
    if (targetdrive >= 'a' && targetdrive <= 'z')
        targetdrive = (char)(targetdrive - 'a' + 'A');

    if (!SetCurrentDirectoryA(path))
        return (O2APIRET)GetLastError();

    if (path[1] == ':' && targetdrive >= 'A' && targetdrive <= 'Z' &&
        olddrive >= 'A' && olddrive <= 'Z' && targetdrive != olddrive) {
        restore[0] = olddrive;
        restore[1] = ':';
        restore[2] = '\0';
        if (!SetCurrentDirectoryA(restore))
            return (O2APIRET)GetLastError();
    }
    return O2_NO_ERROR;
}

/* 256 */
O2APIRET __cdecl DosSetFilePtr(O2HFILE hFile, O2LONG distance,
                              O2ULONG method, O2ULONG *newpos)
{
    return (O2APIRET)os2_core_DosSetFilePtr(
        &o2_personality_context, (os2_handle32_t)hFile,
        (os2_long32_t)distance, (uint32_t)method,
        o2_pointer_address(newpos));
}

/* 257 */
O2APIRET __cdecl DosClose(O2HFILE hFile)
{
    HANDLE h;

    h = os2_handle(hFile);
    if (emx_self_trace_enabled()) {
        fprintf(stderr, "M30B SELF: DosClose hfile=%lu handle=%p\n",
                (unsigned long)hFile, (void *)h);
        fflush(stderr);
    }
    if (h == INVALID_HANDLE_VALUE)
        return O2_ERROR_INVALID_HANDLE;

    if (hFile <= 2) {
        /* Keep the host console handles alive.  The process is exiting anyway. */
        return O2_NO_ERROR;
    }

    if (!CloseHandle(h))
        return (O2APIRET)GetLastError();
    free_os2_handle(hFile);
    return O2_NO_ERROR;
}

/* 239 - 32-bit DosCreatePipe. */
O2APIRET __cdecl DosCreatePipe(O2HFILE *pread, O2HFILE *pwrite, O2ULONG size)
{
    SECURITY_ATTRIBUTES sa;
    HANDLE rh;
    HANDLE wh;
    O2HFILE orh;
    O2HFILE owh;

    if (!pread || !pwrite)
        return O2_ERROR_INVALID_PARAMETER;

    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    rh = INVALID_HANDLE_VALUE;
    wh = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&rh, &wh, &sa, (DWORD)size))
        return (O2APIRET)GetLastError();

    orh = alloc_os2_handle(rh);
    if (orh == (O2HFILE)0xffffffffUL) {
        CloseHandle(rh);
        CloseHandle(wh);
        return O2_ERROR_TOO_MANY_OPEN_FILES;
    }
    owh = alloc_os2_handle(wh);
    if (owh == (O2HFILE)0xffffffffUL) {
        free_os2_handle(orh);
        CloseHandle(rh);
        CloseHandle(wh);
        return O2_ERROR_TOO_MANY_OPEN_FILES;
    }

    *pread = orh;
    *pwrite = owh;
    return O2_NO_ERROR;
}

/* 260 - 32-bit DosDupHandle.
 *
 * A requested target of FFFFFFFF allocates a fresh OS/2 HFILE.  Explicit
 * targets 0/1/2 replace the process standard handle.  For the native-host
 * personality we intentionally do not CloseHandle() the former Win32
 * standard handle here: the host CRT may still have a descriptor referring
 * to it.  cmdos2_win32.c synchronizes that CRT descriptor at the boundary.
 */
O2APIRET __cdecl DosDupHandle(O2HFILE oldFile, O2HFILE *pnewFile)
{
    HANDLE src;
    HANDLE dup;
    HANDLE proc;
    O2HFILE target;

    if (!pnewFile)
        return O2_ERROR_INVALID_PARAMETER;
    src = os2_handle(oldFile);
    if (src == INVALID_HANDLE_VALUE || src == NULL)
        return O2_ERROR_INVALID_HANDLE;

    target = *pnewFile;
    if (target == oldFile)
        return O2_NO_ERROR;

    proc = GetCurrentProcess();
    dup = INVALID_HANDLE_VALUE;
    if (!DuplicateHandle(proc, src, proc, &dup, 0, TRUE,
                         DUPLICATE_SAME_ACCESS))
        return (O2APIRET)GetLastError();

    if (target == (O2HFILE)0xffffffffUL) {
        target = alloc_os2_handle(dup);
        if (target == (O2HFILE)0xffffffffUL) {
            CloseHandle(dup);
            return O2_ERROR_TOO_MANY_OPEN_FILES;
        }
        *pnewFile = target;
        return O2_NO_ERROR;
    }

    if (target <= 2UL) {
        DWORD which;
        HANDLE oldowned;
        if (target == 0UL)
            which = STD_INPUT_HANDLE;
        else if (target == 1UL)
            which = STD_OUTPUT_HANDLE;
        else
            which = STD_ERROR_HANDLE;

        oldowned = o2_std_handles[target];
        if (!SetStdHandle(which, dup)) {
            O2APIRET e;
            e = (O2APIRET)GetLastError();
            CloseHandle(dup);
            return e;
        }

        /* DOSCALLS, not the CRT, owns this exact duplicate.  Keeping the
         * HANDLE itself (rather than a boolean) makes replacement safe even
         * if the native bootstrap has mirrored another duplicate into fd
         * 0/1/2 in the meantime. */
        o2_std_handles[target] = dup;
        if (oldowned != NULL && oldowned != INVALID_HANDLE_VALUE &&
            oldowned != dup)
            CloseHandle(oldowned);

        *pnewFile = target;
        return O2_NO_ERROR;
    }

    if (target >= O2_MAX_HANDLES) {
        CloseHandle(dup);
        return O2_ERROR_INVALID_TARGET_HANDLE;
    }
    if (o2_handles[target] != NULL) {
        CloseHandle(o2_handles[target]);
        o2_handles[target] = NULL;
    }
    o2_handles[target] = dup;
    *pnewFile = target;
    return O2_NO_ERROR;
}

/* 259.  This pre-release CRT passes a second reserved DWORD (zero). */
O2APIRET __cdecl DosDelete(const char *path, O2ULONG reserved)
{
    (void)reserved;
    if (!path)
        return O2_ERROR_INVALID_PARAMETER;
    if (!DeleteFileA(path))
        return (O2APIRET)GetLastError();
    return O2_NO_ERROR;
}

/* 270 - this pre-release runtime uses the DosMkDir2-shaped 3-argument ABI. */
O2APIRET __cdecl DosCreateDir(const char *path, void *pEA, O2ULONG reserved)
{
    (void)pEA;
    (void)reserved;
    if (!path)
        return O2_ERROR_INVALID_PARAMETER;
    if (!CreateDirectoryA(path, NULL))
        return (O2APIRET)GetLastError();
    return O2_NO_ERROR;
}

/* 271 */
O2APIRET __cdecl DosMove(const char *oldPath, const char *newPath)
{
    if (!oldPath || !newPath)
        return O2_ERROR_INVALID_PARAMETER;
    if (!MoveFileA(oldPath, newPath))
        return (O2APIRET)GetLastError();
    return O2_NO_ERROR;
}

/* 272 */
O2APIRET __cdecl DosSetFileSize(O2HFILE hFile, O2ULONG size)
{
    HANDLE h;
    DWORD oldpos;
    DWORD r;
    DWORD err;

    h = os2_handle(hFile);
    if (h == INVALID_HANDLE_VALUE)
        return O2_ERROR_INVALID_HANDLE;

    SetLastError(NO_ERROR);
    oldpos = SetFilePointer(h, 0, NULL, FILE_CURRENT);
    if (oldpos == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
        oldpos = 0xffffffffUL;

    SetLastError(NO_ERROR);
    r = SetFilePointer(h, (LONG)size, NULL, FILE_BEGIN);
    if (r == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
        return (O2APIRET)GetLastError();
    if (!SetEndOfFile(h)) {
        err = GetLastError();
        if (oldpos != 0xffffffffUL)
            (void)SetFilePointer(h, (LONG)oldpos, NULL, FILE_BEGIN);
        return (O2APIRET)err;
    }

    if (oldpos != 0xffffffffUL)
        (void)SetFilePointer(h, (LONG)oldpos, NULL, FILE_BEGIN);
    return O2_NO_ERROR;
}

/*
 * 273 DosOpen.
 *
 * This C/386 runtime uses the nine-argument pre-release/DosOpen2-shaped ABI:
 * path, phFile, pAction, cbFile, attr, openFlags, openMode, pEA, reserved.
 */
O2APIRET __cdecl DosOpen(const char *path, O2HFILE *phFile, O2ULONG *pAction,
                         O2ULONG cbFile, O2ULONG attr, O2ULONG openFlags,
                         O2ULONG openMode, void *pEA, O2ULONG reserved)
{
    DWORD access;
    DWORD share;
    DWORD creation;
    DWORD flags;
    DWORD exists_attr;
    int existed;
    O2ULONG exists_action;
    O2ULONG new_action;
    HANDLE h;
    O2HFILE oh;

    (void)pEA;
    (void)reserved;

    if (!path || !phFile || !pAction)
        return O2_ERROR_INVALID_PARAMETER;

    if (emx_self_trace_enabled()) {
        char full[MAX_PATH];
        DWORD fn;
        fn = GetFullPathNameA(path, (DWORD)sizeof(full), full, NULL);
        fprintf(stderr,
                "M30B SELF: DosOpen path=\"%s\" full=\"%s\" cbFile=%lu attr=%08lX openFlags=%08lX openMode=%08lX\n",
                path, (fn != 0 && fn < (DWORD)sizeof(full)) ? full : "<unresolved>",
                (unsigned long)cbFile, (unsigned long)attr,
                (unsigned long)openFlags, (unsigned long)openMode);
        fflush(stderr);
    }
    if (io_trace_enabled()) {
        char full[MAX_PATH];
        DWORD fn;
        fn = GetFullPathNameA(path, (DWORD)sizeof(full), full, NULL);
        fprintf(stderr,
                "DOSCALLS IO: OPEN path=\"%s\" full=\"%s\" cbFile=%lu attr=%08lX openFlags=%08lX openMode=%08lX\n",
                path, (fn != 0 && fn < (DWORD)sizeof(full)) ? full : "<unresolved>",
                (unsigned long)cbFile, (unsigned long)attr,
                (unsigned long)openFlags, (unsigned long)openMode);
        fflush(stderr);
    }

    switch (openMode & 7UL) {
    case O2_OPEN_ACCESS_READONLY:
        access = GENERIC_READ;
        break;
    case O2_OPEN_ACCESS_WRITEONLY:
        access = GENERIC_WRITE;
        break;
    case O2_OPEN_ACCESS_READWRITE:
        access = GENERIC_READ | GENERIC_WRITE;
        break;
    default:
        return O2_ERROR_INVALID_ACCESS;
    }

    switch (openMode & 0x70UL) {
    case O2_OPEN_SHARE_DENYREADWRITE:
        share = 0;
        break;
    case O2_OPEN_SHARE_DENYWRITE:
        share = FILE_SHARE_READ;
        break;
    case O2_OPEN_SHARE_DENYREAD:
        share = FILE_SHARE_WRITE;
        break;
    case O2_OPEN_SHARE_DENYNONE:
        share = FILE_SHARE_READ | FILE_SHARE_WRITE;
        break;
    default:
        /* Compatibility/default mode: permissive is most useful here. */
        share = FILE_SHARE_READ | FILE_SHARE_WRITE;
        break;
    }

    exists_action = openFlags & 0x0fUL;
    new_action = openFlags & 0xf0UL;

    if (new_action == O2_OPEN_ACTION_CREATE_IF_NEW) {
        if (exists_action == O2_OPEN_ACTION_OPEN_IF_EXISTS)
            creation = OPEN_ALWAYS;
        else if (exists_action == O2_OPEN_ACTION_REPLACE_IF_EXISTS)
            creation = CREATE_ALWAYS;
        else if (exists_action == O2_OPEN_ACTION_FAIL_IF_EXISTS)
            creation = CREATE_NEW;
        else
            return O2_ERROR_INVALID_PARAMETER;
    } else if (new_action == O2_OPEN_ACTION_FAIL_IF_NEW) {
        if (exists_action == O2_OPEN_ACTION_OPEN_IF_EXISTS)
            creation = OPEN_EXISTING;
        else if (exists_action == O2_OPEN_ACTION_REPLACE_IF_EXISTS)
            creation = TRUNCATE_EXISTING;
        else
            return O2_ERROR_INVALID_PARAMETER;
    } else {
        return O2_ERROR_INVALID_PARAMETER;
    }

    exists_attr = GetFileAttributesA(path);
    existed = (exists_attr != 0xffffffffUL);

    flags = win32_attrs_from_o2(attr);
    h = CreateFileA(path, access, share, NULL, creation, flags, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        O2APIRET eopen;
        eopen = (O2APIRET)GetLastError();
        if (emx_self_trace_enabled()) {
            fprintf(stderr, "M30B SELF: DosOpen -> rc=%lu (CreateFile failed)\n",
                    (unsigned long)eopen);
            fflush(stderr);
        }
        if (io_trace_enabled()) {
            fprintf(stderr, "DOSCALLS IO: OPEN -> rc=%lu (CreateFile failed)\n",
                    (unsigned long)eopen);
            fflush(stderr);
        }
        return eopen;
    }

    oh = alloc_os2_handle(h);
    if (oh == (O2HFILE)0xffffffffUL) {
        CloseHandle(h);
        return O2_ERROR_TOO_MANY_OPEN_FILES;
    }

    if (!existed)
        *pAction = 2UL;               /* FILE_CREATED */
    else if (exists_action == O2_OPEN_ACTION_REPLACE_IF_EXISTS)
        *pAction = 3UL;               /* FILE_TRUNCATED/REPLACED */
    else
        *pAction = 1UL;               /* FILE_EXISTED */

    if ((!existed || exists_action == O2_OPEN_ACTION_REPLACE_IF_EXISTS) &&
        cbFile != 0UL) {
        DWORD r;
        SetLastError(NO_ERROR);
        r = SetFilePointer(h, (LONG)cbFile, NULL, FILE_BEGIN);
        if (r == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
            O2APIRET e;
            e = (O2APIRET)GetLastError();
            CloseHandle(h);
            free_os2_handle(oh);
            return e;
        }
        if (!SetEndOfFile(h)) {
            O2APIRET e2;
            e2 = (O2APIRET)GetLastError();
            CloseHandle(h);
            free_os2_handle(oh);
            return e2;
        }
        (void)SetFilePointer(h, 0, NULL, FILE_BEGIN);
    }

    *phFile = oh;
    if (emx_self_trace_enabled()) {
        DWORD sz;
        sz = GetFileSize(h, NULL);
        fprintf(stderr,
                "M30B SELF: DosOpen -> rc=0 hfile=%lu handle=%p action=%lu size=%lu\n",
                (unsigned long)oh, (void *)h, (unsigned long)*pAction,
                (unsigned long)sz);
        fflush(stderr);
    }
    if (io_trace_enabled()) {
        DWORD sz;
        sz = GetFileSize(h, NULL);
        fprintf(stderr,
                "DOSCALLS IO: OPEN -> rc=0 hfile=%lu handle=%p action=%lu size=%lu\n",
                (unsigned long)oh, (void *)h, (unsigned long)*pAction,
                (unsigned long)sz);
        fflush(stderr);
    }
    return O2_NO_ERROR;
}

/* 274 */
O2APIRET __cdecl DosQueryCurrentDir(O2ULONG diskNum, char *buffer,
                                    O2ULONG *pcb)
{
    char cwd[MAX_PATH];
    char spec[4];
    const char *part;
    DWORD n;
    size_t need;

    if (!buffer || !pcb)
        return O2_ERROR_INVALID_PARAMETER;

    if (diskNum == 0UL) {
        n = GetCurrentDirectoryA((DWORD)sizeof(cwd), cwd);
    } else {
        if (diskNum < 1UL || diskNum > 26UL)
            return O2_ERROR_INVALID_PARAMETER;
        spec[0] = (char)('A' + (int)diskNum - 1);
        spec[1] = ':';
        spec[2] = '.';
        spec[3] = '\0';
        n = GetFullPathNameA(spec, (DWORD)sizeof(cwd), cwd, NULL);
    }
    if (n == 0 || n >= (DWORD)sizeof(cwd))
        return (O2APIRET)GetLastError();

    part = cwd;
    if (part[0] != '\0' && part[1] == ':')
        part += 2;
    if (*part == '\\' || *part == '/')
        ++part;
    need = strlen(part) + 1U;
    if ((O2ULONG)need > *pcb) {
        *pcb = (O2ULONG)need;
        return (O2APIRET)ERROR_BUFFER_OVERFLOW;
    }
    memcpy(buffer, part, need);
    *pcb = (O2ULONG)need;
    return O2_NO_ERROR;
}

/* 275 */
O2APIRET __cdecl DosQueryCurrentDisk(O2ULONG *pdisk, O2ULONG *plogical)
{
    char cwd[MAX_PATH];
    DWORD n;
    if (!pdisk || !plogical)
        return O2_ERROR_INVALID_PARAMETER;
    n = GetCurrentDirectoryA((DWORD)sizeof(cwd), cwd);
    if (n == 0 || n >= (DWORD)sizeof(cwd))
        return (O2APIRET)GetLastError();
    if (cwd[0] >= 'a' && cwd[0] <= 'z')
        cwd[0] = (char)(cwd[0] - 'a' + 'A');
    if (cwd[0] < 'A' || cwd[0] > 'Z' || cwd[1] != ':')
        return O2_ERROR_INVALID_PARAMETER;
    *pdisk = (O2ULONG)(cwd[0] - 'A' + 1);
    *plogical = (O2ULONG)GetLogicalDrives();
    return O2_NO_ERROR;
}

/* 279 */
O2APIRET __cdecl DosQueryFileInfo(O2HFILE hFile, O2ULONG level,
                                  void *buffer, O2ULONG cb)
{
    HANDLE h;

    if (!buffer)
        return O2_ERROR_INVALID_PARAMETER;
    if (level != O2_FIL_STANDARD || cb < 24UL)
        return O2_ERROR_INVALID_PARAMETER;

    h = os2_handle(hFile);
    if (h == INVALID_HANDLE_VALUE)
        return O2_ERROR_INVALID_HANDLE;

    {
        O2APIRET rc;
        rc = fill_fil_standard_handle((unsigned char *)buffer, h);
        if (io_trace_enabled()) {
            const unsigned char *q = (const unsigned char *)buffer;
            O2ULONG fsize = 0;
            if (rc == O2_NO_ERROR && cb >= 16UL)
                fsize = (O2ULONG)q[12] | ((O2ULONG)q[13] << 8) |
                        ((O2ULONG)q[14] << 16) | ((O2ULONG)q[15] << 24);
            fprintf(stderr,
                    "DOSCALLS IO: QUERYFILEINFO hfile=%lu level=%lu cb=%lu -> rc=%lu size=%lu\n",
                    (unsigned long)hFile, (unsigned long)level,
                    (unsigned long)cb, (unsigned long)rc, (unsigned long)fsize);
            fflush(stderr);
        }
        if (emx_self_trace_enabled()) {
            fprintf(stderr,
                    "M30B SELF: DosQueryFileInfo hfile=%lu level=%lu cb=%lu -> rc=%lu",
                    (unsigned long)hFile, (unsigned long)level,
                    (unsigned long)cb, (unsigned long)rc);
            if (rc == O2_NO_ERROR) {
                const unsigned char *q;
                O2ULONG fsize;
                q = (const unsigned char *)buffer;
                fsize = (O2ULONG)q[12] | ((O2ULONG)q[13] << 8) |
                        ((O2ULONG)q[14] << 16) | ((O2ULONG)q[15] << 24);
                fprintf(stderr, " size=%lu bytes=", (unsigned long)fsize);
                trace_hex16(q, cb < 24UL ? cb : 24UL);
            }
            fprintf(stderr, "\n");
            fflush(stderr);
        }
        return rc;
    }
}

/* Append a Windows command-line argument with quotes around it.  This is
 * deliberately small and C89-friendly.  It is sufficient for executable
 * paths/program names; the caller appends the OS/2 parameter tail verbatim. */
static int append_quoted_arg(char *dst, unsigned cap, const char *arg)
{
    unsigned used;
    unsigned need;
    const char *p;

    if (!dst || !arg || cap == 0U)
        return 0;
    used = (unsigned)strlen(dst);
    need = 2U; /* surrounding quotes */
    p = arg;
    while (*p) {
        /* A literal quote needs a backslash for the Win32 CRT parser. */
        if (*p == '"')
            ++need;
        ++need;
        ++p;
    }
    if (used + need + 1U > cap)
        return 0;

    dst[used++] = '"';
    p = arg;
    while (*p) {
        if (*p == '"')
            dst[used++] = '\\';
        dst[used++] = *p++;
    }
    dst[used++] = '"';
    dst[used] = '\0';
    return 1;
}

static int append_text(char *dst, unsigned cap, const char *text)
{
    unsigned used;
    unsigned n;

    if (!dst || !text || cap == 0U)
        return 0;
    used = (unsigned)strlen(dst);
    n = (unsigned)strlen(text);
    if (used + n + 1U > cap)
        return 0;
    memcpy(dst + used, text, n + 1U);
    return 1;
}

/* 281 */
O2APIRET __cdecl DosRead(O2HFILE hFile, void *buffer,
                         O2ULONG count, O2ULONG *actual)
{
    DWORD done;
    DWORD e;
    HANDLE h;

    if (!actual || (!buffer && count != 0UL))
        return O2_ERROR_INVALID_PARAMETER;
    h = os2_handle(hFile);
    if (h == INVALID_HANDLE_VALUE)
        return O2_ERROR_INVALID_HANDLE;
    done = 0;
    if (emx_self_trace_enabled()) {
        DWORD before;
        SetLastError(NO_ERROR);
        before = SetFilePointer(h, 0, NULL, FILE_CURRENT);
        fprintf(stderr, "M30B SELF: DosRead hfile=%lu pos=%lu ask=%lu\n",
                (unsigned long)hFile, (unsigned long)before,
                (unsigned long)count);
        fflush(stderr);
    }
    if (!ReadFile(h, buffer, (DWORD)count, &done, NULL)) {
        e = GetLastError();
        *actual = (O2ULONG)done;
        if (io_trace_enabled()) {
            fprintf(stderr,
                    "DOSCALLS IO: READ hfile=%lu handle=%p native=%s ask=%lu -> err=%lu actual=%lu\n",
                    (unsigned long)hFile, (void *)h, win_handle_type_name(h),
                    (unsigned long)count, (unsigned long)e,
                    (unsigned long)*actual);
            fflush(stderr);
        }
        return (O2APIRET)e;
    }
    *actual = (O2ULONG)done;
    if (io_trace_enabled()) {
        fprintf(stderr,
                "DOSCALLS IO: READ hfile=%lu handle=%p native=%s ask=%lu -> rc=0 actual=%lu\n",
                (unsigned long)hFile, (void *)h, win_handle_type_name(h),
                (unsigned long)count, (unsigned long)*actual);
        fflush(stderr);
    }
    if (emx_self_trace_enabled()) {
        DWORD after;
        after = SetFilePointer(h, 0, NULL, FILE_CURRENT);
        fprintf(stderr, "M30B SELF: DosRead -> rc=0 actual=%lu pos_after=%lu bytes=",
                (unsigned long)*actual, (unsigned long)after);
        if (buffer && *actual != 0UL)
            trace_hex16((const unsigned char *)buffer, *actual);
        fprintf(stderr, "\n");
        fflush(stderr);
    }
    return O2_NO_ERROR;
}

/* 282 */
O2APIRET __cdecl DosWrite(O2HFILE hFile, const void *buffer,
                          O2ULONG count, O2ULONG *actual)
{
    return (O2APIRET)os2_core_DosWrite(
        &o2_personality_context, (os2_handle32_t)hFile,
        o2_pointer_address(buffer), (uint32_t)count,
        o2_pointer_address(actual));
}

/* 283 - direct-LX process implementation.
 *
 * IMPORTANT: the calling thread is running on the OS/2 guest stack.  Real
 * OS/2 programs commonly have quite small stacks (the first regression
 * program has only about 10 KB).  Calling a heavyweight Win32 routine such
 * as CreateProcessA directly from that stack can exhaust it.
 *
 * Therefore process creation/waiting is performed on a native Win32 worker
 * thread.  The worker receives a heap-resident command line and has a normal
 * Windows thread stack.  The guest thread only creates/waits for the worker.
 */
/*
 * CreateProcessA is stricter about a caller-supplied environment than OS/2
 * DosExecPgm.  In particular, Windows expects a double-NUL-terminated list of
 * NAME=VALUE strings sorted case-insensitively; the hidden per-drive entries
 * use the special =X:=path form.  The C/386 runtime gives CMD a perfectly
 * usable OS/2 environment, but it does not promise Windows' ordering.
 *
 * Normalize a private copy at the personality boundary.  This preserves CMD's
 * private SET/SETLOCAL environment without mutating the hosting Win32 process.
 */
static int o2_env_entry_valid(const char *s)
{
    const char *eq;

    if (s == NULL || *s == '\0')
        return 0;

    if (s[0] == '=') {
        /* Win32 environment blocks may contain hidden current-directory
         * pseudo-variables whose names themselves begin with '='.  The
         * familiar form is =C:=C:\\path, but Windows can also expose
         * pseudo-drive names such as =::=::\\.  Treat the first '=' as
         * part of the name and require a later '=' separator. */
        eq = strchr(s + 1, '=');
        return eq != NULL && eq > s + 1;
    }

    eq = strchr(s, '=');
    return eq != NULL && eq != s;
}

static char *o2_normalize_environment(const char *env,
                                      SIZE_T *bytesOut,
                                      O2APIRET *errorOut)
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
        *bytesOut = 0;
    if (errorOut != NULL)
        *errorOut = O2_NO_ERROR;
    if (env == NULL)
        return NULL;

    p = env;
    bytes = 0;
    count = 0;
    for (;;) {
        len = strlen(p);
        if (len == 0) {
            ++bytes; /* extra NUL terminating the block */
            break;
        }
        if (!o2_env_entry_valid(p)) {
            if (exec_trace_enabled()) {
                fprintf(stderr,
                        "DOSCALLS EXEC: invalid environment entry [%s]\n", p);
                fflush(stderr);
            }
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

    /* Empty environment is represented by two NUL bytes. */
    if (count == 0) {
        out = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 2U);
        if (out == NULL) {
            if (errorOut != NULL)
                *errorOut = 8UL;
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
            *errorOut = 8UL;
        return NULL;
    }

    p = env;
    for (i = 0; i < count; ++i) {
        items[i] = (char *)p;
        p += strlen(p) + 1U;
    }

    /* Small, deterministic insertion sort; environment blocks have only a
       modest number of entries and this avoids CRT qsort callback baggage. */
    for (i = 1; i < count; ++i) {
        tmp = items[i];
        j = i;
        while (j != 0 && lstrcmpiA(items[j - 1], tmp) > 0) {
            items[j] = items[j - 1];
            --j;
        }
        items[j] = tmp;
    }

    /* bytes already includes the block's final extra NUL.  The last ordinary
       entry contributed its own terminating NUL. */
    out = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes);
    if (out == NULL) {
        HeapFree(GetProcessHeap(), 0, items);
        if (errorOut != NULL)
            *errorOut = 8UL;
        return NULL;
    }

    dst = out;
    for (i = 0; i < count; ++i) {
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

static void o2_translate_process_exit(DWORD exitCode,
                                      O2ULONG *terminateCode,
                                      O2ULONG *resultCode)
{
    if (terminateCode == NULL || resultCode == NULL)
        return;

    *terminateCode = O2_TC_EXIT;
    *resultCode = (O2ULONG)exitCode;

    /* Ctrl+C is delivered to an OS/2 2.x 32-bit program as a signal
     * exception.  If the host child lets the corresponding Win32 event
     * terminate it, report an abnormal 32-bit exception termination rather
     * than exposing Windows' STATUS_CONTROL_C_EXIT as an application result. */
    if ((O2ULONG)exitCode == O2_WIN_STATUS_CONTROL_C_EXIT) {
        *terminateCode = O2_TC_EXCEPTION;
        *resultCode = 0UL;
    }
}

struct O2ExecWorker {
    char *command;
    const char *env;
    SIZE_T envBytes;
    O2ULONG execFlag;
    O2APIRET rc;
    O2ULONG terminateCode;
    O2ULONG resultCode;
    HANDLE process;
    DWORD pid;
    HANDLE stdInput;
    HANDLE stdOutput;
    HANDLE stdError;
};

static DWORD WINAPI o2_exec_worker_thread(LPVOID arg)
{
    struct O2ExecWorker *w;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD exitCode;
    DWORD creationFlags;
    BOOL ok;
    HANDLE backgroundNull;
    SECURITY_ATTRIBUTES nullSa;
    int backgroundNullUsed;

    w = (struct O2ExecWorker *)arg;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    creationFlags = 0;
    backgroundNull = NULL;
    backgroundNullUsed = 0;
    si.dwFlags = STARTF_USESTDHANDLES;

    /* M28D4: process inheritance follows the OS/2 HFILE table, not the
     * temporary Win32 CRT mirror used by the native CMD bootstrap.
     *
     * cmdos2_win32.c may duplicate HFILE 0/1/2 again so MSVCRT fd 0/1/2
     * can use them.  GetStdHandle() therefore describes bootstrap plumbing,
     * whereas os2_handle(0/1/2) is the personality's authoritative state.
     * Snapshot those exact HANDLEs before the worker thread is started and
     * pass them here through STARTUPINFO. */
    si.hStdInput = w->stdInput;
    si.hStdOutput = w->stdOutput;
    si.hStdError = w->stdError;

    /* EXEC_BACKGROUND is the OS/2 detached-process mode.  Create the
     * nested loader without a console association and do not retain a wait
     * handle.  Redirected inheritable file/pipe handles remain available;
     * console I/O from a detached OS/2 program is outside the documented
     * contract anyway. */
    if (w->execFlag == O2_EXEC_BACKGROUND) {
        creationFlags = DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP;

        /* A detached Win32 console process has no console of its own.
         * Passing inherited CONIN$/CONOUT$ handles into it creates confusing
         * half-valid standard streams.  Preserve real redirected files/pipes,
         * but substitute an inheritable NUL handle for console character
         * handles.  This keeps DETACH quiet while still allowing
         * `detach child >file` to work exactly through the OS/2 HFILE table. */
        ZeroMemory(&nullSa, sizeof(nullSa));
        nullSa.nLength = sizeof(nullSa);
        nullSa.bInheritHandle = TRUE;
        if (GetFileType(si.hStdInput) == FILE_TYPE_CHAR ||
            GetFileType(si.hStdOutput) == FILE_TYPE_CHAR ||
            GetFileType(si.hStdError) == FILE_TYPE_CHAR) {
            backgroundNull = CreateFileA("NUL",
                                         GENERIC_READ | GENERIC_WRITE,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                                         &nullSa, OPEN_EXISTING,
                                         FILE_ATTRIBUTE_NORMAL, NULL);
            if (backgroundNull != INVALID_HANDLE_VALUE) {
                if (GetFileType(si.hStdInput) == FILE_TYPE_CHAR)
                    si.hStdInput = backgroundNull;
                if (GetFileType(si.hStdOutput) == FILE_TYPE_CHAR)
                    si.hStdOutput = backgroundNull;
                if (GetFileType(si.hStdError) == FILE_TYPE_CHAR)
                    si.hStdError = backgroundNull;
                backgroundNullUsed = 1;
            } else {
                backgroundNull = NULL;
            }
        }
    }

    if (si.hStdInput != NULL && si.hStdInput != INVALID_HANDLE_VALUE)
        (void)SetHandleInformation(si.hStdInput, HANDLE_FLAG_INHERIT,
                                   HANDLE_FLAG_INHERIT);
    if (si.hStdOutput != NULL && si.hStdOutput != INVALID_HANDLE_VALUE)
        (void)SetHandleInformation(si.hStdOutput, HANDLE_FLAG_INHERIT,
                                   HANDLE_FLAG_INHERIT);
    if (si.hStdError != NULL && si.hStdError != INVALID_HANDLE_VALUE)
        (void)SetHandleInformation(si.hStdError, HANDLE_FLAG_INHERIT,
                                   HANDLE_FLAG_INHERIT);

    if (exec_trace_enabled()) {
        fprintf(stderr,
                "DOSCALLS EXEC: command=[%s]\n"
                "DOSCALLS EXEC: env=%s bytes=%lu flag=%lu create=0x%08lX stdin=%p(%s) stdout=%p(%s) stderr=%p(%s)\n",
                w->command,
                w->env != NULL ? "custom" : "inherit",
                (unsigned long)w->envBytes,
                (unsigned long)w->execFlag,
                (unsigned long)creationFlags,
                (void *)si.hStdInput, win_handle_type_name(si.hStdInput),
                (void *)si.hStdOutput, win_handle_type_name(si.hStdOutput),
                (void *)si.hStdError, win_handle_type_name(si.hStdError));
        if (backgroundNullUsed)
            fprintf(stderr, "DOSCALLS EXEC: detached console std handles -> NUL (redirected files/pipes preserved)\n");
        fflush(stderr);
    }

    ok = CreateProcessA(NULL,
                        w->command,
                        NULL,
                        NULL,
                        TRUE,
                        creationFlags,
                        (LPVOID)w->env,
                        NULL,
                        &si,
                        &pi);
    if (backgroundNull != NULL) {
        CloseHandle(backgroundNull);
        backgroundNull = NULL;
    }
    if (!ok) {
        w->rc = (O2APIRET)GetLastError();
        if (exec_trace_enabled()) {
            fprintf(stderr, "DOSCALLS EXEC: CreateProcessA failed rc=%lu\n",
                    (unsigned long)w->rc);
            fflush(stderr);
        }
        return 0;
    }

    CloseHandle(pi.hThread);
    w->pid = pi.dwProcessId;

    if (w->execFlag == O2_EXEC_SYNC) {
        if (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_OBJECT_0) {
            w->rc = (O2APIRET)GetLastError();
            CloseHandle(pi.hProcess);
            return 0;
        }

        exitCode = 0;
        if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
            w->rc = (O2APIRET)GetLastError();
            CloseHandle(pi.hProcess);
            return 0;
        }
        CloseHandle(pi.hProcess);

        w->rc = O2_NO_ERROR;
        o2_translate_process_exit(exitCode,
                                  &w->terminateCode,
                                  &w->resultCode);
        if (exec_trace_enabled()) {
            fprintf(stderr,
                    "DOSCALLS EXEC: child pid=%lu exit=0x%08lX term=%lu result=%lu\n",
                    (unsigned long)w->pid,
                    (unsigned long)exitCode,
                    (unsigned long)w->terminateCode,
                    (unsigned long)w->resultCode);
            fflush(stderr);
        }
        return 0;
    }

    /* OS/2 returns the child PID through codeTerminate for asynchronous
     * DosExecPgm.  EXEC_ASYNCRESULT keeps the native process handle so a
     * later DosWaitChild can collect the termination/result codes. */
    w->rc = O2_NO_ERROR;
    w->terminateCode = (O2ULONG)pi.dwProcessId;
    w->resultCode = 0UL;
    if (w->execFlag == O2_EXEC_ASYNCRESULT) {
        w->process = pi.hProcess;
    } else {
        CloseHandle(pi.hProcess);
        w->process = NULL;
    }
    return 0;
}

static int program_is_native_cmd_bootstrap(const char *program)
{
    const char *base;
    const char *p;

    if (program == NULL)
        return 0;
    base = program;
    p = program;
    while (*p != '\0') {
        if (*p == '\\' || *p == '/')
            base = p + 1;
        ++p;
    }
    return lstrcmpiA(base, "cmd32os2.exe") == 0;
}

O2APIRET __cdecl DosExecPgm(char *objectName, O2LONG objectNameLen,
                            O2ULONG execFlag, const char *args,
                            const char *env,
                            struct O2ResultCodes *results,
                            const char *program)
{
    char host[MAX_PATH];
    char verbose[8];
    char *command;
    const char *tail;
    const char *argv0;
    const char *runSwitch;
    DWORD hostLen;
    DWORD verboseLen;
    HANDLE worker;
    DWORD workerId;
    struct O2ExecWorker *work;
    O2APIRET rc;
    O2APIRET envRc;
    char *normalizedEnv;
    SIZE_T normalizedEnvBytes;
    int nativeBootstrap;

    if (!results || !program || !*program)
        return O2_ERROR_INVALID_PARAMETER;
    results->codeTerminate = 0UL;
    results->codeResult = 0UL;

    if (execFlag != O2_EXEC_SYNC &&
        execFlag != O2_EXEC_ASYNC &&
        execFlag != O2_EXEC_ASYNCRESULT &&
        execFlag != O2_EXEC_BACKGROUND)
        return O2_ERROR_INVALID_FUNCTION;

    nativeBootstrap = program_is_native_cmd_bootstrap(program);
    host[0] = '\0';
    runSwitch = "";

    if (!nativeBootstrap) {
        /* Normally the host is the current os2host32.exe process.  A native
         * personality front-end can point the compatibility layer at the
         * real LX loader explicitly. */
        hostLen = GetEnvironmentVariableA("OS2HOST32_LOADER",
                                          host, (DWORD)sizeof(host));
        if (hostLen == 0) {
            hostLen = GetModuleFileNameA(NULL, host, (DWORD)sizeof(host));
            if (hostLen == 0 || hostLen >= (DWORD)sizeof(host))
                return (O2APIRET)GetLastError();
        } else if (hostLen >= (DWORD)sizeof(host)) {
            return O2_ERROR_INVALID_PARAMETER;
        }

        verboseLen = GetEnvironmentVariableA("OS2HOST32_VERBOSE",
                                             verbose, (DWORD)sizeof(verbose));
        if (verboseLen != 0 && verboseLen < (DWORD)sizeof(verbose) &&
            verbose[0] != '0')
            runSwitch = " --run ";
        else
            runSwitch = " --run-quiet ";
    }

    argv0 = NULL;
    if (args != NULL && args[0] != '\0')
        argv0 = args;

    command = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                (SIZE_T)O2_EXEC_CMDLINE_MAX);
    if (command == NULL)
        return 8UL; /* ERROR_NOT_ENOUGH_MEMORY */

    if (nativeBootstrap) {
        if (!append_quoted_arg(command, O2_EXEC_CMDLINE_MAX, program)) {
            HeapFree(GetProcessHeap(), 0, command);
            return O2_ERROR_INVALID_PARAMETER;
        }
    } else {
        if (!append_quoted_arg(command, O2_EXEC_CMDLINE_MAX, host) ||
            !append_text(command, O2_EXEC_CMDLINE_MAX, runSwitch)) {
            HeapFree(GetProcessHeap(), 0, command);
            return O2_ERROR_INVALID_PARAMETER;
        }
        /* M29N1: preserve the first string of the OS/2 DosExecPgm argument
         * block across the native loader hop.  PgmName is the resolved module
         * path; argv[0] is independently the command token supplied by CMD. */
        if (argv0 != NULL) {
            if (!append_text(command, O2_EXEC_CMDLINE_MAX, "--argv0 ") ||
                !append_quoted_arg(command, O2_EXEC_CMDLINE_MAX, argv0) ||
                !append_text(command, O2_EXEC_CMDLINE_MAX, " ")) {
                HeapFree(GetProcessHeap(), 0, command);
                return O2_ERROR_INVALID_PARAMETER;
            }
        }
        if (!append_quoted_arg(command, O2_EXEC_CMDLINE_MAX, program)) {
            HeapFree(GetProcessHeap(), 0, command);
            return O2_ERROR_INVALID_PARAMETER;
        }
    }

    tail = NULL;
    if (args != NULL) {
        /* Skip argv[0] in the native OS/2 argument block. */
        tail = args + strlen(args) + 1;
        if (*tail != '\0') {
            if (!append_text(command, O2_EXEC_CMDLINE_MAX, " ") ||
                !append_text(command, O2_EXEC_CMDLINE_MAX, tail)) {
                HeapFree(GetProcessHeap(), 0, command);
                return O2_ERROR_INVALID_PARAMETER;
            }
        }
    }

    normalizedEnv = NULL;
    normalizedEnvBytes = 0;
    envRc = O2_NO_ERROR;
    if (env != NULL) {
        normalizedEnv = o2_normalize_environment(env,
                                                 &normalizedEnvBytes,
                                                 &envRc);
        if (normalizedEnv == NULL) {
            HeapFree(GetProcessHeap(), 0, command);
            return envRc != O2_NO_ERROR ? envRc : O2_ERROR_INVALID_PARAMETER;
        }
    }

    work = (struct O2ExecWorker *)HeapAlloc(GetProcessHeap(),
                                             HEAP_ZERO_MEMORY,
                                             sizeof(*work));
    if (work == NULL) {
        if (normalizedEnv != NULL)
            HeapFree(GetProcessHeap(), 0, normalizedEnv);
        HeapFree(GetProcessHeap(), 0, command);
        return 8UL;
    }
    work->command = command;
    work->env = normalizedEnv;
    work->envBytes = normalizedEnvBytes;
    work->execFlag = execFlag;
    work->rc = O2_ERROR_INVALID_FUNCTION;
    work->process = NULL;
    work->pid = 0;

    /* Capture the OS/2 standard HFILEs while the caller's redirection is
     * still in force.  DosExecPgm waits for the native creation worker to
     * finish before returning, but taking the snapshot here also removes any
     * dependence on process-global GetStdHandle() state inside that worker. */
    work->stdInput = os2_handle(0UL);
    work->stdOutput = os2_handle(1UL);
    work->stdError = os2_handle(2UL);

    if (work->stdInput == NULL || work->stdInput == INVALID_HANDLE_VALUE ||
        work->stdOutput == NULL || work->stdOutput == INVALID_HANDLE_VALUE ||
        work->stdError == NULL || work->stdError == INVALID_HANDLE_VALUE) {
        HeapFree(GetProcessHeap(), 0, work);
        if (normalizedEnv != NULL)
            HeapFree(GetProcessHeap(), 0, normalizedEnv);
        HeapFree(GetProcessHeap(), 0, command);
        return O2_ERROR_INVALID_HANDLE;
    }

    workerId = 0;
    worker = CreateThread(NULL, 0, o2_exec_worker_thread,
                          (LPVOID)work, 0, &workerId);
    if (worker == NULL) {
        rc = (O2APIRET)GetLastError();
        HeapFree(GetProcessHeap(), 0, work);
        if (normalizedEnv != NULL)
            HeapFree(GetProcessHeap(), 0, normalizedEnv);
        HeapFree(GetProcessHeap(), 0, command);
        return rc;
    }

    if (WaitForSingleObject(worker, INFINITE) != WAIT_OBJECT_0) {
        rc = (O2APIRET)GetLastError();
        CloseHandle(worker);
        HeapFree(GetProcessHeap(), 0, work);
        if (normalizedEnv != NULL)
            HeapFree(GetProcessHeap(), 0, normalizedEnv);
        HeapFree(GetProcessHeap(), 0, command);
        return rc;
    }
    CloseHandle(worker);

    rc = work->rc;
    if (rc == O2_NO_ERROR && execFlag == O2_EXEC_ASYNCRESULT) {
        if (!store_child_process(work->process, work->pid)) {
            if (work->process != NULL)
                CloseHandle(work->process);
            work->process = NULL;
            rc = O2_ERROR_TOO_MANY_OPEN_FILES;
        }
    }

    if (rc == O2_NO_ERROR) {
        results->codeTerminate = work->terminateCode;
        results->codeResult = work->resultCode;
        if (objectName != NULL && objectNameLen > 0)
            objectName[0] = '\0';
    } else if (objectName != NULL && objectNameLen > 0) {
        size_t n;
        n = strlen(program);
        if (n >= (size_t)objectNameLen)
            n = (size_t)objectNameLen - 1U;
        memcpy(objectName, program, n);
        objectName[n] = '\0';
    }

    HeapFree(GetProcessHeap(), 0, work);
    if (normalizedEnv != NULL)
        HeapFree(GetProcessHeap(), 0, normalizedEnv);
    HeapFree(GetProcessHeap(), 0, command);
    return rc;
}

/* 280 - wait for an asynchronous child created with EXEC_ASYNCRESULT. */
O2APIRET __cdecl DosWaitChild(O2ULONG action, O2ULONG option,
                              struct O2ResultCodes *results,
                              O2ULONG *ppid, O2ULONG pid)
{
    HANDLE process;
    DWORD actualPid;
    DWORD wr;
    DWORD exitCode;

    if (!results || !ppid)
        return O2_ERROR_INVALID_PARAMETER;
    if (action != O2_DCWA_PROCESS)
        return O2_ERROR_INVALID_FUNCTION;
    if (option != O2_DCWW_WAIT && option != O2_DCWW_NOWAIT)
        return O2_ERROR_INVALID_PARAMETER;

    results->codeTerminate = 0UL;
    results->codeResult = 0UL;
    *ppid = 0UL;

    actualPid = 0;
    process = take_child_process((DWORD)pid, &actualPid);
    if (process == NULL)
        return O2_ERROR_WAIT_NO_CHILDREN;

    wr = WaitForSingleObject(process,
                             option == O2_DCWW_WAIT ? INFINITE : 0);
    if (wr == WAIT_TIMEOUT) {
        put_child_process_back(process, actualPid);
        return O2_ERROR_CHILD_NOT_COMPLETE;
    }
    if (wr != WAIT_OBJECT_0) {
        O2APIRET e;
        e = (O2APIRET)GetLastError();
        CloseHandle(process);
        return e;
    }

    exitCode = 0;
    if (!GetExitCodeProcess(process, &exitCode)) {
        O2APIRET e2;
        e2 = (O2APIRET)GetLastError();
        CloseHandle(process);
        return e2;
    }
    CloseHandle(process);

    o2_translate_process_exit(exitCode,
                              &results->codeTerminate,
                              &results->codeResult);
    *ppid = (O2ULONG)actualPid;
    if (exec_trace_enabled()) {
        fprintf(stderr,
                "DOSCALLS WAIT: child pid=%lu exit=0x%08lX term=%lu result=%lu\n",
                (unsigned long)actualPid,
                (unsigned long)exitCode,
                (unsigned long)results->codeTerminate,
                (unsigned long)results->codeResult);
        fflush(stderr);
    }
    return O2_NO_ERROR;
}

static O2ULONG o2_sub_align8(O2ULONG cb)
{
    if (cb > 0xfffffff8UL)
        return 0;
    return (cb + 7UL) & ~7UL;
}

static struct O2SubPool *o2_find_subpool(void *base)
{
    unsigned int i;
    for (i = 0; i < O2_MAX_SUBPOOLS; ++i) {
        if (o2_subpools[i].in_use && o2_subpools[i].base == base)
            return &o2_subpools[i];
    }
    return NULL;
}

static struct O2SubPool *o2_new_subpool(void *base)
{
    unsigned int i;
    for (i = 0; i < O2_MAX_SUBPOOLS; ++i) {
        if (!o2_subpools[i].in_use) {
            memset(&o2_subpools[i], 0, sizeof(o2_subpools[i]));
            o2_subpools[i].in_use = 1;
            o2_subpools[i].base = base;
            return &o2_subpools[i];
        }
    }
    return NULL;
}

static void o2_sub_coalesce(struct O2SubPool *pool)
{
    unsigned int i;
    if (!pool)
        return;
    i = 0;
    while (i + 1U < pool->nranges) {
        struct O2SubRange *a;
        struct O2SubRange *b;
        a = &pool->ranges[i];
        b = &pool->ranges[i + 1U];
        if (a->is_free && b->is_free && a->off + a->len == b->off) {
            unsigned int j;
            a->len += b->len;
            for (j = i + 1U; j + 1U < pool->nranges; ++j)
                pool->ranges[j] = pool->ranges[j + 1U];
            --pool->nranges;
        } else {
            ++i;
        }
    }
}

/* 344 - DosSubSetMem. */
O2APIRET __cdecl DosSubSetMem(void *base, O2ULONG flags, O2ULONG size)
{
    struct O2SubPool *pool;
    MEMORY_BASIC_INFORMATION mbi;
    O2ULONG usable;

    if (!base || size < O2_SUBPOOL_HEADER)
        return O2_ERROR_INVALID_PARAMETER;

    /* Do not let a malformed guest pool description walk beyond its mapped
     * memory object.  VirtualQuery is also useful for LX/static objects. */
    memset(&mbi, 0, sizeof(mbi));
    if (VirtualQuery(base, &mbi, sizeof(mbi)) == 0)
        return O2_ERROR_INVALID_PARAMETER;
    if ((unsigned char *)base < (unsigned char *)mbi.BaseAddress ||
        (SIZE_T)size > mbi.RegionSize -
                         (SIZE_T)((unsigned char *)base - (unsigned char *)mbi.BaseAddress))
        return O2_ERROR_INVALID_PARAMETER;

    pool = o2_find_subpool(base);
    if (!pool) {
        /* DOSSUB_INIT is required by real OS/2 for a private new pool.  Be a
         * little tolerant of vintage runtimes that carry extra flag bits, but
         * still reject an explicit GROW of a non-existent pool. */
        if ((flags & O2_DOSSUB_GROW) && !(flags & O2_DOSSUB_INIT))
            return O2_ERROR_INVALID_PARAMETER;
        pool = o2_new_subpool(base);
        if (!pool)
            return O2_ERROR_NOT_ENOUGH_MEMORY;
        pool->size = size;
        pool->flags = flags;
        usable = o2_sub_align8(size - O2_SUBPOOL_HEADER);
        if (usable > size - O2_SUBPOOL_HEADER)
            usable -= 8UL;
        pool->nranges = 1;
        pool->ranges[0].off = O2_SUBPOOL_HEADER;
        pool->ranges[0].len = usable;
        pool->ranges[0].is_free = 1;
        /* The first 64 bytes are reserved by OS/2's pool manager.  Zeroing
         * them is deterministic and matches a freshly initialized pool well
         * enough for clients which only use the documented DosSub* API. */
        memset(base, 0, (size_t)O2_SUBPOOL_HEADER);
    } else {
        if (size < pool->size)
            return O2_ERROR_INVALID_PARAMETER;
        if (size > pool->size) {
            O2ULONG old_aligned;
            O2ULONG new_aligned;
            O2ULONG extra;
            unsigned int n;
            old_aligned = pool->size & ~7UL;
            new_aligned = size & ~7UL;
            extra = new_aligned > old_aligned ? new_aligned - old_aligned : 0;
            if (extra != 0) {
                n = pool->nranges;
                if (n != 0 && pool->ranges[n - 1U].is_free &&
                    pool->ranges[n - 1U].off + pool->ranges[n - 1U].len == old_aligned) {
                    pool->ranges[n - 1U].len += extra;
                } else {
                    if (n >= O2_MAX_SUBRANGES)
                        return O2_ERROR_NOT_ENOUGH_MEMORY;
                    pool->ranges[n].off = old_aligned;
                    pool->ranges[n].len = extra;
                    pool->ranges[n].is_free = 1;
                    ++pool->nranges;
                }
            }
            pool->size = size;
        }
        pool->flags |= flags;
    }

    if (emx_self_trace_enabled()) {
        fprintf(stderr,
                "M30I SUBMEM: DosSubSetMem base=%p flags=%08lX size=%lu ranges=%u\n",
                base, (unsigned long)flags, (unsigned long)size,
                pool->nranges);
    }
    return O2_NO_ERROR;
}

/* 345 - DosSubAllocMem. */
O2APIRET __cdecl DosSubAllocMem(void *base, void **ppBlock, O2ULONG size)
{
    struct O2SubPool *pool;
    O2ULONG need;
    unsigned int i;

    if (!base || !ppBlock || size == 0)
        return O2_ERROR_INVALID_PARAMETER;
    pool = o2_find_subpool(base);
    if (!pool)
        return O2_ERROR_INVALID_PARAMETER;
    need = o2_sub_align8(size);
    if (need == 0)
        return O2_ERROR_NOT_ENOUGH_MEMORY;

    for (i = 0; i < pool->nranges; ++i) {
        struct O2SubRange *r;
        r = &pool->ranges[i];
        if (!r->is_free || r->len < need)
            continue;
        if (r->len > need) {
            unsigned int j;
            if (pool->nranges >= O2_MAX_SUBRANGES)
                return O2_ERROR_NOT_ENOUGH_MEMORY;
            for (j = pool->nranges; j > i + 1U; --j)
                pool->ranges[j] = pool->ranges[j - 1U];
            pool->ranges[i + 1U].off = r->off + need;
            pool->ranges[i + 1U].len = r->len - need;
            pool->ranges[i + 1U].is_free = 1;
            ++pool->nranges;
            r = &pool->ranges[i];
            r->len = need;
        }
        r->is_free = 0;
        *ppBlock = (void *)((unsigned char *)base + r->off);
        if (emx_self_trace_enabled()) {
            fprintf(stderr,
                    "M30I SUBMEM: DosSubAllocMem base=%p ask=%lu rounded=%lu -> %p\n",
                    base, (unsigned long)size, (unsigned long)need, *ppBlock);
        }
        return O2_NO_ERROR;
    }
    *ppBlock = NULL;
    return O2_ERROR_NOT_ENOUGH_MEMORY;
}

/* 346 - DosSubFreeMem. */
O2APIRET __cdecl DosSubFreeMem(void *base, void *block, O2ULONG size)
{
    struct O2SubPool *pool;
    O2ULONG off;
    O2ULONG cb;
    unsigned int i;

    if (!base || !block || size == 0)
        return O2_ERROR_INVALID_PARAMETER;
    pool = o2_find_subpool(base);
    if (!pool || (unsigned char *)block < (unsigned char *)base)
        return O2_ERROR_INVALID_PARAMETER;
    off = (O2ULONG)((unsigned char *)block - (unsigned char *)base);
    cb = o2_sub_align8(size);
    for (i = 0; i < pool->nranges; ++i) {
        struct O2SubRange *r;
        r = &pool->ranges[i];
        if (r->off == off && !r->is_free) {
            /* OS/2 callers supply the original allocation size.  Accept an
             * equivalent 8-byte-rounded size, reject obviously wrong frees. */
            if (cb == 0 || cb != r->len)
                return O2_ERROR_INVALID_PARAMETER;
            r->is_free = 1;
            o2_sub_coalesce(pool);
            if (emx_self_trace_enabled()) {
                fprintf(stderr,
                        "M30I SUBMEM: DosSubFreeMem base=%p block=%p size=%lu\n",
                        base, block, (unsigned long)size);
            }
            return O2_NO_ERROR;
        }
    }
    return O2_ERROR_INVALID_PARAMETER;
}

/* 347 - DosSubUnsetMem. */
O2APIRET __cdecl DosSubUnsetMem(void *base)
{
    struct O2SubPool *pool;
    if (!base)
        return O2_ERROR_INVALID_PARAMETER;
    pool = o2_find_subpool(base);
    if (!pool)
        return O2_ERROR_INVALID_PARAMETER;
    if (emx_self_trace_enabled())
        fprintf(stderr, "M30I SUBMEM: DosSubUnsetMem base=%p\n", base);
    memset(pool, 0, sizeof(*pool));
    return O2_NO_ERROR;
}

/* 299: pre-release C/386 ABI has a fourth reserved argument. */
O2APIRET __cdecl DosAllocMem(void **ppBase, O2ULONG size,
                             O2ULONG flags, O2ULONG reserved)
{
    return (O2APIRET)os2_core_DosAllocMem(
        &o2_personality_context, o2_pointer_address(ppBase),
        (uint32_t)size, (uint32_t)flags, (uint32_t)reserved);
}

/* 304 */
O2APIRET __cdecl DosFreeMem(void *base)
{
    return (O2APIRET)os2_core_DosFreeMem(
        &o2_personality_context, o2_pointer_address(base));
}

/* 305 */
O2APIRET __cdecl DosSetMem(void *base, O2ULONG size, O2ULONG flags)
{
    return (O2APIRET)os2_core_DosSetMem(
        &o2_personality_context, o2_pointer_address(base),
        (uint32_t)size, (uint32_t)flags);
}

/* 306 - query attributes for a contiguous virtual-memory range. */
O2APIRET __cdecl DosQueryMem(void *base, O2ULONG *pcb, O2ULONG *pflags)
{
    MEMORY_BASIC_INFORMATION mbi;
    unsigned char *start;
    unsigned char *region_end;
    O2ULONG available;
    O2ULONG flags;
    DWORD prot;

    if (!base || !pcb || !pflags || *pcb == 0)
        return O2_ERROR_INVALID_PARAMETER;
    if (VirtualQuery(base, &mbi, sizeof(mbi)) == 0)
        return (O2APIRET)GetLastError();

    start = (unsigned char *)base;
    region_end = (unsigned char *)mbi.BaseAddress + mbi.RegionSize;
    if (region_end <= start)
        return O2_ERROR_INVALID_PARAMETER;
    if ((SIZE_T)(region_end - start) > 0xffffffffUL)
        available = 0xffffffffUL;
    else
        available = (O2ULONG)(region_end - start);
    if (*pcb < available)
        available = *pcb;

    flags = 0;
    if (mbi.State == MEM_FREE) {
        flags |= O2_PAG_FREE;
    } else {
        if (mbi.State == MEM_COMMIT)
            flags |= O2_PAG_COMMIT;
        prot = mbi.Protect & 0xffUL;
        if (prot == PAGE_READONLY || prot == PAGE_READWRITE ||
            prot == PAGE_WRITECOPY || prot == PAGE_EXECUTE_READ ||
            prot == PAGE_EXECUTE_READWRITE || prot == PAGE_EXECUTE_WRITECOPY)
            flags |= O2_PAG_READ;
        if (prot == PAGE_READWRITE || prot == PAGE_WRITECOPY ||
            prot == PAGE_EXECUTE_READWRITE || prot == PAGE_EXECUTE_WRITECOPY)
            flags |= O2_PAG_WRITE;
        if (prot == PAGE_EXECUTE || prot == PAGE_EXECUTE_READ ||
            prot == PAGE_EXECUTE_READWRITE || prot == PAGE_EXECUTE_WRITECOPY)
            flags |= O2_PAG_EXECUTE;
        if (mbi.Protect & PAGE_GUARD)
            flags |= O2_PAG_GUARD;
        if (start == (unsigned char *)mbi.AllocationBase)
            flags |= O2_PAG_BASE;
    }

    *pcb = available;
    *pflags = flags;
    return O2_NO_ERROR;
}

/* 348 */

/* M29N2b.1: runtime module APIs are implemented by the module manager that
 * lives inside OS2HOST32.EXE.  DOSCALLS is a separate PE DLL in the same
 * process, so resolve a deliberately tiny private bridge from the process
 * image rather than maintaining a second, divergent module table here. */
typedef O2APIRET (__cdecl *O2HOSTMODLOAD)(const char *, O2ULONG *, char *, O2ULONG);
typedef O2APIRET (__cdecl *O2HOSTMODQUERYHANDLE)(const char *, O2ULONG *);
typedef O2APIRET (__cdecl *O2HOSTMODQUERYNAME)(O2ULONG, O2ULONG, char *);
typedef O2APIRET (__cdecl *O2HOSTMODQUERYPROC)(O2ULONG, O2ULONG, const char *, O2ULONG *);
typedef O2APIRET (__cdecl *O2HOSTMODFREE)(O2ULONG);
typedef void (__cdecl *O2HOSTMODTERMINATEALL)(void);

struct O2HostModuleBridge {
    int tried;
    O2HOSTMODLOAD load;
    O2HOSTMODQUERYHANDLE query_handle;
    O2HOSTMODQUERYNAME query_name;
    O2HOSTMODQUERYPROC query_proc;
    O2HOSTMODFREE free_module;
    O2HOSTMODTERMINATEALL terminate_all;
};

static struct O2HostModuleBridge g_o2_module_bridge;

static int o2_resolve_module_bridge(void)
{
    HMODULE exe;
    if (g_o2_module_bridge.tried)
        return g_o2_module_bridge.load != 0 &&
               g_o2_module_bridge.query_handle != 0 &&
               g_o2_module_bridge.query_name != 0 &&
               g_o2_module_bridge.query_proc != 0 &&
               g_o2_module_bridge.free_module != 0 &&
               g_o2_module_bridge.terminate_all != 0;
    g_o2_module_bridge.tried = 1;
    exe = GetModuleHandleA(NULL);
    if (!exe)
        return 0;
    g_o2_module_bridge.load = (O2HOSTMODLOAD)GetProcAddress(exe, "OS2HostModuleLoad");
    g_o2_module_bridge.query_handle = (O2HOSTMODQUERYHANDLE)GetProcAddress(exe, "OS2HostModuleQueryHandle");
    g_o2_module_bridge.query_name = (O2HOSTMODQUERYNAME)GetProcAddress(exe, "OS2HostModuleQueryName");
    g_o2_module_bridge.query_proc = (O2HOSTMODQUERYPROC)GetProcAddress(exe, "OS2HostModuleQueryProc");
    g_o2_module_bridge.free_module = (O2HOSTMODFREE)GetProcAddress(exe, "OS2HostModuleFree");
    g_o2_module_bridge.terminate_all = (O2HOSTMODTERMINATEALL)GetProcAddress(exe, "OS2HostModuleTerminateAll");
    return g_o2_module_bridge.load != 0 &&
           g_o2_module_bridge.query_handle != 0 &&
           g_o2_module_bridge.query_name != 0 &&
           g_o2_module_bridge.query_proc != 0 &&
           g_o2_module_bridge.free_module != 0 &&
           g_o2_module_bridge.terminate_all != 0;
}

static void o2_module_process_exit(void)
{
    if (o2_resolve_module_bridge() && g_o2_module_bridge.terminate_all)
        g_o2_module_bridge.terminate_all();
}

/* 318 - DosLoadModule. */
O2APIRET __cdecl DosLoadModule(char *objectName, O2ULONG objectNameLen,
                               const char *moduleName, O2ULONG *moduleHandle)
{
    if (!moduleName || !moduleHandle)
        return O2_ERROR_INVALID_PARAMETER;
    if (objectName && objectNameLen != 0)
        objectName[0] = 0;
    if (!o2_resolve_module_bridge())
        return O2_ERROR_INVALID_FUNCTION;
    return g_o2_module_bridge.load(moduleName, moduleHandle,
                                   objectName, objectNameLen);
}

/* 319 - DosQueryModuleHandle (the 32-bit successor of DosGetModHandle). */
O2APIRET __cdecl DosQueryModuleHandle(const char *moduleName,
                                      O2ULONG *moduleHandle)
{
    if (!moduleName || !moduleHandle)
        return O2_ERROR_INVALID_PARAMETER;
    if (!o2_resolve_module_bridge())
        return O2_ERROR_INVALID_FUNCTION;
    return g_o2_module_bridge.query_handle(moduleName, moduleHandle);
}

/* 320 - DosQueryModuleName.  HMODULE 1 is the synthetic main guest image
 * advertised through PIB.pib_hmte; ordinary guest DLL handles are resolved by
 * the same OS2HOST32 module graph used for imports and DosLoadModule. */
O2APIRET __cdecl DosQueryModuleName(O2ULONG moduleHandle, O2ULONG cb,
                                    char *buffer)
{
    if (!buffer || cb == 0)
        return O2_ERROR_INVALID_PARAMETER;
    if (!o2_resolve_module_bridge())
        return O2_ERROR_INVALID_FUNCTION;
    {
        O2APIRET rc;
        rc = g_o2_module_bridge.query_name(moduleHandle, cb, buffer);
        if (emx_self_trace_enabled()) {
            char full[MAX_PATH];
            DWORD fn;
            full[0] = 0;
            fn = (rc == O2_NO_ERROR)
                 ? GetFullPathNameA(buffer, (DWORD)sizeof(full), full, NULL) : 0;
            fprintf(stderr,
                    "M30B SELF: DosQueryModuleName hmod=%08lX cb=%lu -> rc=%lu name=\"%s\" full=\"%s\"\n",
                    (unsigned long)moduleHandle, (unsigned long)cb,
                    (unsigned long)rc, rc == O2_NO_ERROR ? buffer : "",
                    (fn != 0 && fn < (DWORD)sizeof(full)) ? full : "<unresolved>");
            fflush(stderr);
        }
        return rc;
    }
}

/* 321 - DosQueryProcAddr (the 32-bit successor of DosGetProcAddr). */
O2APIRET __cdecl DosQueryProcAddr(O2ULONG moduleHandle, O2ULONG ordinal,
                                  const char *procName, void **procAddress)
{
    O2ULONG addr;
    O2APIRET rc;
    if (!procAddress || (ordinal == 0 && (!procName || !*procName)))
        return O2_ERROR_INVALID_PARAMETER;
    if (!o2_resolve_module_bridge())
        return O2_ERROR_INVALID_FUNCTION;
    addr = 0;
    rc = g_o2_module_bridge.query_proc(moduleHandle, ordinal, procName, &addr);
    if (rc == O2_NO_ERROR)
        *procAddress = (void *)(ULONG_PTR)addr;
    return rc;
}

/* 322 - DosFreeModule. */
O2APIRET __cdecl DosFreeModule(O2ULONG moduleHandle)
{
    if (!o2_resolve_module_bridge())
        return O2_ERROR_INVALID_FUNCTION;
    return g_o2_module_bridge.free_module(moduleHandle);
}

/* 354 - DosSetExceptionHandler.
 *
 * OS/2's EXCEPTIONREGISTRATIONRECORD is two 32-bit words:
 *   +0 previous registration record
 *   +4 exception-handler entry point
 *
 * Linking the record is enough for ordinary EMX startup.  Actual host-fault
 * translation into the OS/2 exception dispatcher is deliberately deferred.
 */
O2APIRET __cdecl DosSetExceptionHandler(struct O2ExceptionRegistrationRecord *rec)
{
    if (!rec)
        return O2_ERROR_INVALID_PARAMETER;

    rec->prev_structure = o2_exception_head;
    o2_exception_head = rec;
    o2_info_tib.pexchain = rec;

    if (emx_self_trace_enabled()) {
        fprintf(stderr,
                "M30E EXCEPT: DosSetExceptionHandler rec=%08lX prev=%08lX handler=%08lX\n",
                (unsigned long)(ULONG_PTR)rec,
                (unsigned long)(ULONG_PTR)rec->prev_structure,
                (unsigned long)(ULONG_PTR)rec->exception_handler);
    }
    return O2_NO_ERROR;
}

/* 355 - matching chain teardown.  The supplied EMX DLL does not currently
 * import this ordinal statically, but keeping the inverse operation correct
 * prevents a future dynamically-resolved or later-runtime teardown from
 * leaving a stale registration record behind.
 */
O2APIRET __cdecl DosUnsetExceptionHandler(struct O2ExceptionRegistrationRecord *rec)
{
    struct O2ExceptionRegistrationRecord *p;
    struct O2ExceptionRegistrationRecord *next;
    unsigned guard;

    if (!rec)
        return O2_ERROR_INVALID_PARAMETER;

    if (o2_exception_head == rec) {
        o2_exception_head = rec->prev_structure;
    } else {
        p = o2_exception_head;
        guard = 0U;
        while (p != NULL &&
               p != (struct O2ExceptionRegistrationRecord *)(ULONG_PTR)0xffffffffUL &&
               guard++ < 64U) {
            next = p->prev_structure;
            if (next == rec) {
                p->prev_structure = rec->prev_structure;
                break;
            }
            p = next;
        }
    }
    o2_info_tib.pexchain = o2_exception_head;

    if (emx_self_trace_enabled()) {
        fprintf(stderr,
                "M30E EXCEPT: DosUnsetExceptionHandler rec=%08lX newhead=%08lX\n",
                (unsigned long)(ULONG_PTR)rec,
                (unsigned long)(ULONG_PTR)o2_exception_head);
    }
    return O2_NO_ERROR;
}

/* 378 - DosSetSignalExceptionFocus.
 *
 * Full-screen/console OS/2 applications acquire signal-exception focus to
 * receive Ctrl-C and Ctrl-Break as exceptions.  The API is nestable and
 * returns the resulting acquire-minus-release count through pulTimes.
 *
 * M30F models the bookkeeping faithfully for the current single-console
 * guest.  Actual Win32 console-control delivery into the guest exception
 * chain is intentionally deferred until a program requires it.
 */
O2APIRET __cdecl DosSetSignalExceptionFocus(O2ULONG enable, O2ULONG *pulTimes)
{
    if (!pulTimes || (enable != 0U && enable != 1U))
        return O2_ERROR_INVALID_PARAMETER;

    if (enable != 0U) {
        if (o2_signal_exception_focus_count != 0xffffffffUL)
            ++o2_signal_exception_focus_count;
    } else if (o2_signal_exception_focus_count != 0U) {
        --o2_signal_exception_focus_count;
    }

    *pulTimes = o2_signal_exception_focus_count;

    if (emx_self_trace_enabled()) {
        fprintf(stderr,
                "M30F SIGNAL: DosSetSignalExceptionFocus enable=%lu times=%lu\n",
                (unsigned long)enable,
                (unsigned long)o2_signal_exception_focus_count);
    }
    return O2_NO_ERROR;
}

/* 382 - DosSetRelMaxFH.
 *
 * pcbReqCount is a signed delta.  Zero queries the current HFILE ceiling.
 * OS/2 never reduces the process below 20 handles and must preserve handles
 * which are already open.  The M30 host has a fixed 256-slot HFILE table, so
 * growth is faithfully accepted up to that implementation ceiling.
 */
O2APIRET __cdecl DosSetRelMaxFH(O2LONG *pcbReqCount, O2ULONG *pcbCurMaxFH)
{
    O2LONG req;
    O2LONG wanted;
    O2ULONG floor;
    O2ULONG i;

    if (!pcbReqCount || !pcbCurMaxFH)
        return O2_ERROR_INVALID_PARAMETER;

    req = *pcbReqCount;
    floor = 20U;
    for (i = 3U; i < O2_MAX_HANDLES; ++i) {
        if (o2_handles[i] != NULL && i + 1U > floor)
            floor = i + 1U;
    }

    if (req > 0) {
        if ((O2ULONG)req >= O2_MAX_HANDLES - o2_max_file_handles)
            wanted = O2_MAX_HANDLES;
        else
            wanted = (O2LONG)o2_max_file_handles + req;
        o2_max_file_handles = (O2ULONG)wanted;
    } else if (req < 0) {
        if (req <= -(O2LONG)o2_max_file_handles)
            wanted = 0;
        else
            wanted = (O2LONG)o2_max_file_handles + req;
        if (wanted < (O2LONG)floor)
            wanted = (O2LONG)floor;
        if (wanted < 20)
            wanted = 20;
        o2_max_file_handles = (O2ULONG)wanted;
    }

    *pcbCurMaxFH = o2_max_file_handles;

    if (emx_self_trace_enabled()) {
        fprintf(stderr,
                "M30H FILES: DosSetRelMaxFH delta=%ld -> max=%lu (floor=%lu)\n",
                (long)req, (unsigned long)o2_max_file_handles,
                (unsigned long)floor);
    }
    return O2_NO_ERROR;
}

/* 418 - DosAcknowledgeSignalException.
 *
 * Acknowledging a delivered signal tells OS/2 that subsequent occurrences
 * may be dispatched.  M30 does not deliver host console controls into the
 * guest yet, so there is no pending bit to clear; accepting the acknowledgement
 * is nevertheless the correct normal-path behavior for EMX initialization.
 */
O2APIRET __cdecl DosAcknowledgeSignalException(O2ULONG signalNum)
{
    if (emx_self_trace_enabled()) {
        fprintf(stderr,
                "M30F SIGNAL: DosAcknowledgeSignalException signal=%08lX\n",
                (unsigned long)signalNum);
    }
    return O2_NO_ERROR;
}

O2APIRET __cdecl DosQuerySysInfo(O2ULONG first, O2ULONG last,
                                  void *buffer, O2ULONG cb)
{
    return (O2APIRET)os2_core_DosQuerySysInfo(
        &o2_personality_context, (uint32_t)first, (uint32_t)last,
        o2_pointer_address(buffer), (uint32_t)cb);
}

/* Process-owned NLS state. DOSCALLS.289/291 and NLS.5/6/7 all reach this
 * same context, so a codepage switch is immediately visible across modules. */
O2APIRET __cdecl DosSetProcessCp(O2ULONG codepage)
{
    return (O2APIRET)os2_nls_api_DosSetProcessCp(
        &o2_personality_context, (uint32_t)codepage);
}

O2APIRET __cdecl DosQueryCp(O2ULONG cb, O2ULONG *codepages, O2ULONG *actual)
{
    return (O2APIRET)os2_nls_api_DosQueryCp(
        &o2_personality_context, (uint32_t)cb, o2_pointer_address(codepages),
        o2_pointer_address(actual));
}

/* Named helpers are private personality plumbing imported by NLS.DLL. */
O2APIRET __cdecl O2NlsQueryCtryInfo(O2ULONG cb, const void *countrycode,
                                     void *countryinfo, O2ULONG *actual)
{
    return (O2APIRET)os2_nls_api_DosQueryCtryInfo(
        &o2_personality_context, (uint32_t)cb, o2_pointer_address(countrycode),
        o2_pointer_address(countryinfo), o2_pointer_address(actual));
}

O2APIRET __cdecl O2NlsQueryDBCSEnv(O2ULONG cb, const void *countrycode,
                                    void *buffer)
{
    return (O2APIRET)os2_nls_api_DosQueryDBCSEnv(
        &o2_personality_context, (uint32_t)cb, o2_pointer_address(countrycode),
        o2_pointer_address(buffer));
}

O2APIRET __cdecl O2NlsMapCase(O2ULONG cb, const void *countrycode, void *buffer)
{
    return (O2APIRET)os2_nls_api_DosMapCase(
        &o2_personality_context, (uint32_t)cb, o2_pointer_address(countrycode),
        o2_pointer_address(buffer));
}

/* OS/2 also publishes these NLS functions through DOSCALLS.395-.397. */
O2APIRET __cdecl DosQueryCtryInfo(O2ULONG cb, const void *countrycode,
                                   void *countryinfo, O2ULONG *actual)
{
    return O2NlsQueryCtryInfo(cb, countrycode, countryinfo, actual);
}

O2APIRET __cdecl DosQueryDBCSEnv(O2ULONG cb, const void *countrycode,
                                  void *buffer)
{
    return O2NlsQueryDBCSEnv(cb, countrycode, buffer);
}

O2APIRET __cdecl DosMapCase(O2ULONG cb, const void *countrycode, void *buffer)
{
    return O2NlsMapCase(cb, countrycode, buffer);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    O2ULONG i;
    (void)instance;

    if (reason == DLL_PROCESS_ATTACH) {
        os2_nls_state_init(&o2_nls_state);
        os2_win32_initialize_nls(&o2_nls_state);
        os2_personality_context_set_nls(&o2_personality_context, &o2_nls_state);
        o2_exception_head =
            (struct O2ExceptionRegistrationRecord *)(ULONG_PTR)0xffffffffUL;
        for (i = 0; i < O2_MAX_HANDLES; ++i)
            o2_handles[i] = NULL;
        for (i = 0; i < 3UL; ++i)
            o2_std_handles[i] = NULL;
        for (i = 0; i < O2_MAX_CHILDREN; ++i) {
            o2_children[i].process = NULL;
            o2_children[i].pid = 0;
        }
        InitializeCriticalSection(&child_lock);
        child_lock_ready = 1;
        InitializeCriticalSection(&beep_lock);
        beep_lock_ready = 1;
        beep_event = NULL;
        beep_wave = NULL;
        beep_samples = NULL;
        beep_sample_capacity = 0UL;
        beep_phase = 0UL;
    } else if (reason == DLL_PROCESS_DETACH) {
        /* If reserved is non-NULL Windows is terminating the process and will
           reclaim multimedia/kernel objects itself.  Avoid doing loader-lock
           work in that case. */
        if (reserved == NULL) {
            if (beep_wave != NULL) {
                waveOutReset(beep_wave);
                waveOutClose(beep_wave);
                beep_wave = NULL;
            }
            if (beep_event != NULL) {
                CloseHandle(beep_event);
                beep_event = NULL;
            }
            if (beep_samples != NULL) {
                HeapFree(GetProcessHeap(), 0, beep_samples);
                beep_samples = NULL;
                beep_sample_capacity = 0UL;
            }
        }
        if (reserved == NULL) {
            for (i = 0; i < 3UL; ++i) {
                if (o2_std_handles[i] != NULL &&
                    o2_std_handles[i] != INVALID_HANDLE_VALUE) {
                    CloseHandle(o2_std_handles[i]);
                    o2_std_handles[i] = NULL;
                }
            }
        }
        if (reserved == NULL && child_lock_ready) {
            for (i = 0; i < O2_MAX_CHILDREN; ++i) {
                if (o2_children[i].process != NULL) {
                    CloseHandle(o2_children[i].process);
                    o2_children[i].process = NULL;
                    o2_children[i].pid = 0;
                }
            }
        }
        if (child_lock_ready) {
            DeleteCriticalSection(&child_lock);
            child_lock_ready = 0;
        }
        if (beep_lock_ready) {
            DeleteCriticalSection(&beep_lock);
            beep_lock_ready = 0;
        }
    }
    return TRUE;
}

/* M31F JIGSAW - ordinal 236, DosSetPriority (source-era DosSetPrty alias). */
O2APIRET __cdecl DosSetPriority(O2ULONG scope, O2ULONG prtyClass,
                                O2LONG delta, O2ULONG porTid)
{
    HANDLE th = GetCurrentThread();
    int wp = THREAD_PRIORITY_NORMAL;
    (void)prtyClass;
    /* JIGSAW changes the current thread only (PRTYS_THREAD, tid 0).  Keep
       process/tree requests harmless until a specimen needs broader scope. */
    if (scope == 2UL && porTid == 0UL) {
        if (delta <= -16) wp = THREAD_PRIORITY_LOWEST;
        else if (delta < 0) wp = THREAD_PRIORITY_BELOW_NORMAL;
        else if (delta >= 16) wp = THREAD_PRIORITY_HIGHEST;
        else if (delta > 0) wp = THREAD_PRIORITY_ABOVE_NORMAL;
        SetThreadPriority(th, wp);
    }
    return 0;
}
