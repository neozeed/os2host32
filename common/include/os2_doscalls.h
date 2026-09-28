#ifndef OS2_DOSCALLS_H
#define OS2_DOSCALLS_H

/*
 * Backend-neutral native DOSCALLS session.
 *
 * The exported DOSCALLS DLL preserves the historical C/386 ABI while this
 * layer owns OS/2 process/personality state.  The Win32 implementation is a
 * backend; Win32 HANDLE/DWORD/FILETIME types must not appear in this header.
 *
 * This native-session layer complements os2_doscalls_core.h.  The latter is
 * the address-space-neutral core shared with WHP.  R2 deliberately keeps that
 * proven WHP interface intact.
 */

#include <stddef.h>
#include <stdint.h>

#include "os2_personality.h"
#include "os2_nls.h"

#ifndef __cdecl
#define __cdecl
#endif

typedef uint32_t O2APIRET;
typedef uint32_t O2ULONG;
typedef int32_t  O2LONG;
typedef uint32_t O2HFILE;
typedef void (__cdecl *O2THREADFN)(O2ULONG);
typedef uintptr_t O2NATIVE;

struct O2ResultCodes {
    O2ULONG codeTerminate;
    O2ULONG codeResult;
};

struct O2ExceptionRegistrationRecord {
    struct O2ExceptionRegistrationRecord *prev_structure;
    void *exception_handler;
};

#define O2_NO_ERROR                    0UL
#define O2_ERROR_INVALID_FUNCTION       1UL
#define O2_ERROR_TOO_MANY_OPEN_FILES    4UL
#define O2_ERROR_INVALID_HANDLE         6UL
#define O2_ERROR_NOT_ENOUGH_MEMORY      8UL
#define O2_ERROR_INVALID_ACCESS        12UL
#define O2_ERROR_BUFFER_OVERFLOW      111UL
#define O2_ERROR_INVALID_TARGET_HANDLE 114UL
#define O2_ERROR_INVALID_CATEGORY     117UL
#define O2_ERROR_WAIT_NO_CHILDREN     128UL
#define O2_ERROR_CHILD_NOT_COMPLETE   129UL
#define O2_ERROR_SEM_NOT_FOUND        187UL
#define O2_ERROR_INVALID_EXE_SIGNATURE 191UL
#define O2_ERROR_ENVVAR_NOT_FOUND     203UL
#define O2_ERROR_DUPLICATE_NAME       285UL
#define O2_ERROR_TOO_MANY_OPENS       291UL
#define O2_ERROR_INIT_ROUTINE_FAILED  295UL
#define O2_ERROR_ALREADY_POSTED       299UL
#define O2_ERROR_ALREADY_RESET        300UL
#define O2_ERROR_INVALID_FREQUENCY    395UL
#define O2_ERROR_INVALID_PARAMETER     87UL
#define O2_ERROR_MOD_NOT_FOUND        126UL
#define O2_ERROR_PROC_NOT_FOUND       127UL

#define O2_DOS_MAX_HANDLES       256U
#define O2_DOS_MAX_FIND_HANDLES   64U
#define O2_DOS_MAX_CHILDREN       64U
#define O2_DOS_MAX_SUBPOOLS       32U
#define O2_DOS_MAX_SUBRANGES     256U
#define O2_DOS_SIGNAL_SLOTS      8U
#define O2_DOS_VECTOR_SLOTS      32U
#define O2_DOS_EVENT_SLOTS        64U
#define O2_DOS_EVENT_NAME_MAX    127U
#define O2_DOS_SUBPOOL_HEADER     64UL
#define O2_DOS_SEM_INDEFINITE_WAIT 0xffffffffUL
#define O2_DOS_NATIVE_INVALID ((O2NATIVE)0)

struct Os2DosBackendOps;

struct Os2DosSubRange {
    O2ULONG off;
    O2ULONG len;
    int is_free;
};

struct Os2DosSubPool {
    void *base;
    O2ULONG size;
    O2ULONG flags;
    int in_use;
    unsigned int nranges;
    struct Os2DosSubRange ranges[O2_DOS_MAX_SUBRANGES];
};

struct Os2DosChild {
    O2NATIVE process;
    O2ULONG pid;
};

struct Os2DosEventSem {
    O2NATIVE native_event;
    O2ULONG generation;
    O2ULONG refs;
    O2ULONG post_count;
    char name[O2_DOS_EVENT_NAME_MAX + 1U];
};

/* Historical 16-bit signal/vector state used by Microsoft C/386 and EMX
 * mixed-mode runtimes.  Handler values are opaque 32-bit far-pointer tokens;
 * common DOSCALLS never dereferences them. */
struct Os2DosSignalSlot {
    O2ULONG handler;
    unsigned short action;
};

struct Os2DosVectorSlot {
    unsigned short vector;
    O2ULONG handler;
    int in_use;
};

struct Os2DosSession {
    void *backend_opaque;
    const struct Os2DosBackendOps *backend;
    int initialized;

    /* OS/2 HFILE namespace.  0/1/2 are logical standard handles; backend
     * tokens are stored only when DOSCALLS itself owns a replacement. */
    O2NATIVE file_handles[O2_DOS_MAX_HANDLES];
    O2NATIVE std_handles[3];
    O2ULONG max_file_handles;

    /* OS/2-visible handle namespaces are personality state, not Win32 state. */
    O2NATIVE find_handles[O2_DOS_MAX_FIND_HANDLES];
    struct Os2DosChild children[O2_DOS_MAX_CHILDREN];
    struct Os2DosEventSem events[O2_DOS_EVENT_SLOTS];

    /* Pure Control Program state. */
    struct Os2DosSubPool subpools[O2_DOS_MAX_SUBPOOLS];
    struct O2ExceptionRegistrationRecord *exception_head;
    O2ULONG signal_exception_focus_count;
    O2ULONG error_flags;

    /* Historical 16-bit process signal and processor-exception vectors. */
    struct Os2DosSignalSlot signal_slots[O2_DOS_SIGNAL_SLOTS];
    struct Os2DosVectorSlot vector_slots[O2_DOS_VECTOR_SLOTS];

    /* Per-process OS/2 National Language Support state.  This is personality
     * state, not backend state; Win32/WHP/OS2SS may only provide bootstrap
     * or resource services underneath it. */
    struct Os2NlsState nls;
};

void os2_dos_session_init(struct Os2DosSession *session,
                          void *backend_opaque,
                          const struct Os2DosBackendOps *backend);
void os2_dos_session_destroy(struct Os2DosSession *session);

/* Internal state helpers used by backends. */
O2APIRET os2_dos_resolve_hfile(struct Os2DosSession *session, O2HFILE hfile,
                               O2NATIVE *native_handle);
O2HFILE os2_dos_alloc_hfile(struct Os2DosSession *session, O2NATIVE native_handle);
void os2_dos_free_hfile(struct Os2DosSession *session, O2HFILE hfile);
O2APIRET os2_dos_replace_hfile(struct Os2DosSession *session, O2HFILE hfile,
                               O2NATIVE native_handle, O2NATIVE *old_handle);
O2NATIVE os2_dos_owned_std_handle(struct Os2DosSession *session, O2HFILE hfile);
O2APIRET os2_dos_set_owned_std_handle(struct Os2DosSession *session,
                                      O2HFILE hfile, O2NATIVE native_handle,
                                      O2NATIVE *old_handle);
O2ULONG os2_dos_alloc_find_handle(struct Os2DosSession *session, O2NATIVE native_handle);
O2APIRET os2_dos_get_find_handle(struct Os2DosSession *session, O2ULONG hdir,
                                 O2NATIVE *native_handle);
void os2_dos_free_find_handle(struct Os2DosSession *session, O2ULONG hdir);
int os2_dos_store_child(struct Os2DosSession *session, O2NATIVE process, O2ULONG pid);
O2NATIVE os2_dos_take_child(struct Os2DosSession *session, O2ULONG wanted_pid,
                            O2ULONG *actual_pid);
void os2_dos_put_child(struct Os2DosSession *session, O2NATIVE process, O2ULONG pid);

/* Public semantic entry points.  Exported DOSCALLS symbols are thin veneers. */
unsigned short os2_dos_DosSetSigHandler(struct Os2DosSession *, void *, void *, unsigned short *, unsigned short, unsigned short);
unsigned short os2_dos_DosSetVec(struct Os2DosSession *, unsigned short, void *, O2ULONG *);
unsigned short os2_dos_DosFlagProcess(struct Os2DosSession *, unsigned short, unsigned short, unsigned short, unsigned short);
O2APIRET os2_dos_DosDevIOCtl(struct Os2DosSession *, O2HFILE, O2ULONG, O2ULONG, void *, O2ULONG, O2ULONG *, void *, O2ULONG, O2ULONG *);
O2APIRET os2_dos_DosBeep(struct Os2DosSession *, O2ULONG, O2ULONG);
O2APIRET os2_dos_Dos16Beep(struct Os2DosSession *, O2ULONG, O2ULONG);
O2APIRET os2_dos_DosSleep(struct Os2DosSession *, O2ULONG);
O2APIRET os2_dos_Dos16Sleep(struct Os2DosSession *, O2ULONG);
O2APIRET os2_dos_DosFindClose(struct Os2DosSession *, O2ULONG);
O2APIRET os2_dos_DosFindFirst(struct Os2DosSession *, const char *, O2ULONG *, O2ULONG, void *, O2ULONG, O2ULONG *, O2ULONG);
O2APIRET os2_dos_DosFindNext(struct Os2DosSession *, O2ULONG, void *, O2ULONG, O2ULONG *);
O2APIRET os2_dos_DosCreateThread(struct Os2DosSession *, O2ULONG *, O2THREADFN, O2ULONG, O2ULONG, O2ULONG);
O2APIRET os2_dos_DosCreateEventSem(struct Os2DosSession *, const char *, O2ULONG *, O2ULONG, O2ULONG);
O2APIRET os2_dos_DosOpenEventSem(struct Os2DosSession *, const char *, O2ULONG *);
O2APIRET os2_dos_DosCloseEventSem(struct Os2DosSession *, O2ULONG);
O2APIRET os2_dos_DosResetEventSem(struct Os2DosSession *, O2ULONG, O2ULONG *);
O2APIRET os2_dos_DosPostEventSem(struct Os2DosSession *, O2ULONG);
O2APIRET os2_dos_DosWaitEventSem(struct Os2DosSession *, O2ULONG, O2ULONG);
O2APIRET os2_dos_DosQueryEventSem(struct Os2DosSession *, O2ULONG, O2ULONG *);
O2APIRET os2_dos_DosCreateMutexSem(struct Os2DosSession *, const char *, O2ULONG *, O2ULONG, O2ULONG);
O2APIRET os2_dos_DosOpenMutexSem(struct Os2DosSession *, const char *, O2ULONG *);
O2APIRET os2_dos_DosCloseMutexSem(struct Os2DosSession *, O2ULONG);
O2APIRET os2_dos_DosRequestMutexSem(struct Os2DosSession *, O2ULONG, O2ULONG);
O2APIRET os2_dos_DosReleaseMutexSem(struct Os2DosSession *, O2ULONG);
O2APIRET os2_dos_DosGetDateTime(struct Os2DosSession *, void *);
O2APIRET os2_dos_DosSetDefaultDisk(struct Os2DosSession *, O2ULONG);
O2APIRET os2_dos_DosScanEnv(struct Os2DosSession *, const char *, char **);
O2APIRET os2_dos_DosSearchPath(struct Os2DosSession *, O2ULONG, const char *, const char *, char *, O2ULONG);
O2APIRET os2_dos_DosGetInfoBlocks(struct Os2DosSession *, void **, void **);
O2APIRET os2_dos_DosError(struct Os2DosSession *, O2ULONG);
O2APIRET os2_dos_DosSetFileInfo(struct Os2DosSession *, O2HFILE, O2ULONG, const void *, O2ULONG);
O2APIRET os2_dos_DosSetPathInfo(struct Os2DosSession *, const char *, O2ULONG, const void *, O2ULONG, O2ULONG);
O2APIRET os2_dos_DosQAppType(struct Os2DosSession *, const char *, O2ULONG *);
O2APIRET os2_dos_DosQueryAppType(struct Os2DosSession *, const char *, O2ULONG *);
O2APIRET os2_dos_DosQueryPathInfo(struct Os2DosSession *, const char *, O2ULONG, void *, O2ULONG);
O2APIRET os2_dos_DosQueryHType(struct Os2DosSession *, O2HFILE, O2ULONG *, O2ULONG *);
O2APIRET os2_dos_DosExitList(struct Os2DosSession *, O2ULONG, void (__cdecl *)(O2ULONG));
O2APIRET os2_dos_DosEnterCritSec(struct Os2DosSession *);
void os2_dos_DosExit(struct Os2DosSession *, O2ULONG, O2ULONG);
O2APIRET os2_dos_DosDeleteDir(struct Os2DosSession *, const char *);
O2APIRET os2_dos_DosSetCurrentDir(struct Os2DosSession *, const char *);
O2APIRET os2_dos_DosSetFilePtr(struct Os2DosSession *, O2HFILE, O2LONG, O2ULONG, O2ULONG *);
O2APIRET os2_dos_DosClose(struct Os2DosSession *, O2HFILE);
O2APIRET os2_dos_DosCreatePipe(struct Os2DosSession *, O2HFILE *, O2HFILE *, O2ULONG);
O2APIRET os2_dos_DosDupHandle(struct Os2DosSession *, O2HFILE, O2HFILE *);
O2APIRET os2_dos_DosDelete(struct Os2DosSession *, const char *, O2ULONG);
O2APIRET os2_dos_DosCreateDir(struct Os2DosSession *, const char *, void *, O2ULONG);
O2APIRET os2_dos_DosMove(struct Os2DosSession *, const char *, const char *);
O2APIRET os2_dos_DosSetFileSize(struct Os2DosSession *, O2HFILE, O2ULONG);
O2APIRET os2_dos_DosOpen(struct Os2DosSession *, const char *, O2HFILE *, O2ULONG *, O2ULONG, O2ULONG, O2ULONG, O2ULONG, void *, O2ULONG);
O2APIRET os2_dos_DosQueryCurrentDir(struct Os2DosSession *, O2ULONG, char *, O2ULONG *);
O2APIRET os2_dos_DosQueryCurrentDisk(struct Os2DosSession *, O2ULONG *, O2ULONG *);
O2APIRET os2_dos_DosQueryFileInfo(struct Os2DosSession *, O2HFILE, O2ULONG, void *, O2ULONG);
O2APIRET os2_dos_DosRead(struct Os2DosSession *, O2HFILE, void *, O2ULONG, O2ULONG *);
O2APIRET os2_dos_DosWrite(struct Os2DosSession *, O2HFILE, const void *, O2ULONG, O2ULONG *);
O2APIRET os2_dos_DosExecPgm(struct Os2DosSession *, char *, O2LONG, O2ULONG, const char *, const char *, struct O2ResultCodes *, const char *);
O2APIRET os2_dos_DosWaitChild(struct Os2DosSession *, O2ULONG, O2ULONG, struct O2ResultCodes *, O2ULONG *, O2ULONG);
O2APIRET os2_dos_DosSubSetMem(struct Os2DosSession *, void *, O2ULONG, O2ULONG);
O2APIRET os2_dos_DosSubAllocMem(struct Os2DosSession *, void *, void **, O2ULONG);
O2APIRET os2_dos_DosSubFreeMem(struct Os2DosSession *, void *, void *, O2ULONG);
O2APIRET os2_dos_DosSubUnsetMem(struct Os2DosSession *, void *);
O2APIRET os2_dos_DosAllocMem(struct Os2DosSession *, void **, O2ULONG, O2ULONG, O2ULONG);
O2APIRET os2_dos_DosFreeMem(struct Os2DosSession *, void *);
O2APIRET os2_dos_DosSetMem(struct Os2DosSession *, void *, O2ULONG, O2ULONG);
O2APIRET os2_dos_DosQueryMem(struct Os2DosSession *, void *, O2ULONG *, O2ULONG *);
O2APIRET os2_dos_DosLoadModule(struct Os2DosSession *, char *, O2ULONG, const char *, O2ULONG *);
O2APIRET os2_dos_DosQueryModuleHandle(struct Os2DosSession *, const char *, O2ULONG *);
O2APIRET os2_dos_DosQueryModuleName(struct Os2DosSession *, O2ULONG, O2ULONG, char *);
O2APIRET os2_dos_DosQueryProcAddr(struct Os2DosSession *, O2ULONG, O2ULONG, const char *, void **);
O2APIRET os2_dos_DosFreeModule(struct Os2DosSession *, O2ULONG);
O2APIRET os2_dos_DosSetExceptionHandler(struct Os2DosSession *, struct O2ExceptionRegistrationRecord *);
O2APIRET os2_dos_DosUnsetExceptionHandler(struct Os2DosSession *, struct O2ExceptionRegistrationRecord *);
O2APIRET os2_dos_DosSetSignalExceptionFocus(struct Os2DosSession *, O2ULONG, O2ULONG *);
O2APIRET os2_dos_DosSetRelMaxFH(struct Os2DosSession *, O2LONG *, O2ULONG *);
O2APIRET os2_dos_DosAcknowledgeSignalException(struct Os2DosSession *, O2ULONG);
O2APIRET os2_dos_DosQuerySysInfo(struct Os2DosSession *, O2ULONG, O2ULONG, void *, O2ULONG);
O2APIRET os2_dos_DosSetProcessCp(struct Os2DosSession *, O2ULONG);
O2APIRET os2_dos_DosQueryCp(struct Os2DosSession *, O2ULONG, O2ULONG *, O2ULONG *);
O2APIRET os2_dos_DosQueryCtryInfo(struct Os2DosSession *, O2ULONG, const void *, void *, O2ULONG *);
O2APIRET os2_dos_DosQueryDBCSEnv(struct Os2DosSession *, O2ULONG, const void *, void *);
O2APIRET os2_dos_DosMapCase(struct Os2DosSession *, O2ULONG, const void *, void *);
O2APIRET os2_dos_DosSetPriority(struct Os2DosSession *, O2ULONG, O2ULONG, O2LONG, O2ULONG);

#endif
