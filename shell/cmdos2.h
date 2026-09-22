#ifndef CMDOS2_H
#define CMDOS2_H

/*
 * cmdos2.h - OS/2-facing service boundary for reconstructed CMD.
 *
 * CMD code calls this interface rather than Win32 directly.  The current
 * native bootstrap backend (cmdos2_win32.c) resolves the real 32-bit OS/2
 * personality ordinals from compatibility DLLs.  The direct C/386/LX backend
 * (cmdos2_os2.c) binds the same service boundary to the historical OS/2 ABI
 * without changing CMD proper.
 *
 * ANSI C89-oriented.
 */

#include <stddef.h>

typedef unsigned long CmdO2Rc;
typedef unsigned long CmdO2Handle;
typedef unsigned long CmdO2FindHandle;

#define CMDO2_NO_ERROR             0UL
#define CMDO2_ERROR_FILE_NOT_FOUND 2UL
#define CMDO2_ERROR_PATH_NOT_FOUND 3UL
#define CMDO2_ERROR_ACCESS_DENIED  5UL
#define CMDO2_ERROR_INVALID_HANDLE 6UL
#define CMDO2_ERROR_NOT_ENOUGH_MEMORY 8UL
#define CMDO2_ERROR_NOT_SAME_DEVICE 17UL
#define CMDO2_ERROR_NO_MORE_FILES 18UL
#define CMDO2_ERROR_ENVVAR_NOT_FOUND 203UL
#define CMDO2_ERROR_FILE_EXISTS   80UL
#define CMDO2_ERROR_INVALID_PARAMETER 87UL
#define CMDO2_ERROR_BUFFER_OVERFLOW 111UL
#define CMDO2_ERROR_WAIT_NO_CHILDREN 128UL
#define CMDO2_ERROR_CHILD_NOT_COMPLETE 129UL
#define CMDO2_ERROR_ALREADY_EXISTS 183UL

#define CMDO2_ATTR_READONLY  0x0001UL
#define CMDO2_ATTR_HIDDEN    0x0002UL
#define CMDO2_ATTR_SYSTEM    0x0004UL
#define CMDO2_ATTR_DIRECTORY 0x0010UL
#define CMDO2_ATTR_ARCHIVED  0x0020UL

#define CMDO2_FIND_CREATE 0xffffffffUL
#define CMDO2_HANDLE_ALLOCATE 0xffffffffUL

#define CMDO2_REDIR_INPUT   1
#define CMDO2_REDIR_OUTPUT  2
#define CMDO2_REDIR_APPEND  3

#define CMDO2_EXEC_SYNC         0UL
#define CMDO2_EXEC_ASYNC        1UL
#define CMDO2_EXEC_ASYNCRESULT  2UL
#define CMDO2_EXEC_BACKGROUND   4UL
#define CMDO2_WAIT_PROCESS      0UL
#define CMDO2_WAIT              0UL
#define CMDO2_NOWAIT            1UL

#define CMDO2_TC_EXIT            0UL
#define CMDO2_TC_HARDERROR       1UL
#define CMDO2_TC_TRAP            2UL
#define CMDO2_TC_KILLPROCESS     3UL
#define CMDO2_TC_EXCEPTION       4UL

/* DosQAppType / DosQueryAppType low three application-type bits. */
#define CMDO2_FAPPTYP_NOTSPEC         0UL
#define CMDO2_FAPPTYP_NOTWINDOWCOMPAT 1UL
#define CMDO2_FAPPTYP_WINDOWCOMPAT    2UL
#define CMDO2_FAPPTYP_WINDOWAPI       3UL
#define CMDO2_FAPPTYP_MASK            7UL

/* M29L session-manager constants used by START / DosStartSession. */
#define CMDO2_SSF_RELATED_INDEPENDENT 0UL
#define CMDO2_SSF_RELATED_CHILD       1UL
#define CMDO2_SSF_FGBG_FORE           0UL
#define CMDO2_SSF_FGBG_BACK           1UL
#define CMDO2_SSF_INHERTOPT_SHELL     0UL
#define CMDO2_SSF_INHERTOPT_PARENT    1UL
#define CMDO2_SSF_TYPE_DEFAULT        0UL
#define CMDO2_SSF_TYPE_FULLSCREEN     1UL
#define CMDO2_SSF_TYPE_WINDOWABLEVIO  2UL
#define CMDO2_SSF_TYPE_PM             3UL

#define CMDO2_SSF_TRACEOPT_NONE       0UL

#define CMDO2_SSF_CONTROL_VISIBLE      0x0000UL
#define CMDO2_SSF_CONTROL_INVISIBLE    0x0001UL
#define CMDO2_SSF_CONTROL_MAXIMIZE     0x0002UL
#define CMDO2_SSF_CONTROL_MINIMIZE     0x0004UL
#define CMDO2_SSF_CONTROL_NOAUTOCLOSE  0x0008UL
#define CMDO2_SSF_CONTROL_SETPOS       0x8000UL

struct CmdO2StdToken {
    unsigned long value;
    unsigned long os2_value;
};

struct CmdO2ResultCodes {
    unsigned long codeTerminate;
    unsigned long codeResult;
};

struct CmdO2FindData {
    unsigned short date;
    unsigned short time;
    unsigned long size;
    unsigned long alloc;
    unsigned long attr;
    char name[260];
};

int CmdO2Init(void);
void CmdO2Done(void);
const char *CmdO2InitError(void);
int CmdO2PrepareLoader(void);

CmdO2Rc CmdO2ExecPgm(char *objectName, long objectNameLen,
                     unsigned long execFlag, const char *args,
                     const char *env, struct CmdO2ResultCodes *results,
                     const char *program);
CmdO2Rc CmdO2WaitChild(unsigned long action, unsigned long option,
                       struct CmdO2ResultCodes *results,
                       unsigned long *pid, unsigned long childPid);
CmdO2Rc CmdO2QueryAppType(const char *program, unsigned long *appType);

/* M29L1 exposes the full 32-bit STARTDATA payload at the CMD boundary.
 * Fields which are pointers remain caller-owned for the duration of the call.
 * ObjectBuffer is an optional caller buffer used for launch-failure context. */
struct CmdO2StartOptions {
    const char *title;
    const char *program;
    const char *inputs;
    const char *termQueue;
    const char *environment;
    unsigned long related;
    unsigned long fgbg;
    unsigned long traceOpt;
    unsigned long inheritOpt;
    unsigned long sessionType;
    const char *iconFile;
    unsigned long pgmHandle;
    unsigned long pgmControl;
    unsigned long initXPos;
    unsigned long initYPos;
    unsigned long initXSize;
    unsigned long initYSize;
    char *objectBuffer;
    unsigned long objectBufferLen;
};

/* Create a new OS/2 session through SESMGR.17.  Unlike DosExecPgm this is
 * not part of the ordinary parent/child DosWaitChild relationship. */
CmdO2Rc CmdO2StartSessionEx(const struct CmdO2StartOptions *options,
                            unsigned long *sessionId, unsigned long *pid);

/* Compatibility wrapper retained for the M29L callers/tests. */
CmdO2Rc CmdO2StartSession(const char *title, const char *program,
                          const char *inputs, const char *env,
                          unsigned long related, unsigned long fgbg,
                          unsigned long sessionType,
                          unsigned long *sessionId, unsigned long *pid);

/* Post-M31G session-control extension used by CMD32 PS/KILL.  The taskId is
 * the SESMGR session id returned by DosStartSession; pid is diagnostic only.
 * QuerySessions deliberately exposes only related child sessions owned by the
 * current shell/session-manager process. */
#define CMDO2_SESSION_TEXT_MAX 260U
#define CMDO2_SESSION_QUERY_MAX 64U
struct CmdO2SessionInfo {
    unsigned long taskId;
    unsigned long pid;
    char program[CMDO2_SESSION_TEXT_MAX];
    char title[CMDO2_SESSION_TEXT_MAX];
};
CmdO2Rc CmdO2QuerySessions(struct CmdO2SessionInfo *entries,
                           unsigned long capacity, unsigned long *count);
CmdO2Rc CmdO2StopSession(unsigned long taskId);

CmdO2Rc CmdO2QueryProgramPath(char *buffer, unsigned long cap);

CmdO2Rc CmdO2OpenRead(const char *path, CmdO2Handle *handle);
CmdO2Rc CmdO2OpenWriteReplace(const char *path, CmdO2Handle *handle);
CmdO2Rc CmdO2Read(CmdO2Handle handle, void *buffer,
                  unsigned long count, unsigned long *actual);
CmdO2Rc CmdO2Write(CmdO2Handle handle, const void *buffer,
                   unsigned long count, unsigned long *actual);
CmdO2Rc CmdO2Close(CmdO2Handle handle);

/* Handle/pipe primitives.  These correspond directly to the 32-bit OS/2
 * DosCreatePipe and DosDupHandle APIs. */
CmdO2Rc CmdO2CreatePipe(CmdO2Handle *readHandle, CmdO2Handle *writeHandle,
                        unsigned long size);
CmdO2Rc CmdO2DupHandle(CmdO2Handle oldHandle, CmdO2Handle *newHandle);

/* Standard-handle redirection boundary used by CMD proper.  The native
 * bootstrap backend may synchronize the host CRT here; the direct OS/2
 * backend implements the same operation with DosDupHandle. */
CmdO2Rc CmdO2StdSave(unsigned long stdHandle, struct CmdO2StdToken *token);
CmdO2Rc CmdO2StdRedirectPath(unsigned long stdHandle, const char *path,
                             int redirType);
CmdO2Rc CmdO2StdRedirectHandle(unsigned long stdHandle, CmdO2Handle source);
CmdO2Rc CmdO2StdRestore(unsigned long stdHandle, struct CmdO2StdToken *token);

/* M29A console boundary.  These are deliberately small CMD-facing wrappers
 * around the OS/2 VIO/KBD personality rather than host console APIs. */
#define CMDO2_IO_WAIT   0UL
#define CMDO2_IO_NOWAIT 1UL

struct CmdO2KbdKeyInfo {
    unsigned char chChar;
    unsigned char chScan;
    unsigned char fbStatus;
    unsigned char bNlsShift;
    unsigned short fsState;
    unsigned long time;
};

CmdO2Rc CmdO2VioWrtTTY(const char *text, unsigned long count);
CmdO2Rc CmdO2VioGetCurPos(unsigned short *row, unsigned short *col);
CmdO2Rc CmdO2VioSetCurPos(unsigned short row, unsigned short col);
CmdO2Rc CmdO2VioGetScreenSize(unsigned short *rows, unsigned short *cols);
CmdO2Rc CmdO2VioClear(void);
CmdO2Rc CmdO2KbdCharIn(struct CmdO2KbdKeyInfo *info, unsigned long wait);
CmdO2Rc CmdO2KbdFlushBuffer(void);

CmdO2Rc CmdO2Delete(const char *path);
CmdO2Rc CmdO2Move(const char *oldPath, const char *newPath);
CmdO2Rc CmdO2CreateDir(const char *path);
CmdO2Rc CmdO2DeleteDir(const char *path);
CmdO2Rc CmdO2SetCurrentDir(const char *path);
CmdO2Rc CmdO2SetDefaultDisk(unsigned long disk);
CmdO2Rc CmdO2QueryCurrentDir(char *buffer, unsigned long cap);
CmdO2Rc CmdO2QueryPathAttr(const char *path, unsigned long *attr);

CmdO2Rc CmdO2FindFirst(const char *pattern, CmdO2FindHandle *handle,
                       struct CmdO2FindData *data);
CmdO2Rc CmdO2FindNext(CmdO2FindHandle handle, struct CmdO2FindData *data);
CmdO2Rc CmdO2FindClose(CmdO2FindHandle handle);


/* CMD-owned process environment.  The block is the classic sequence of
 * NUL-terminated NAME=VALUE strings followed by an extra NUL. */
CmdO2Rc CmdO2EnvGet(const char *name, char *buffer, unsigned long cap);
CmdO2Rc CmdO2EnvSet(const char *name, const char *value);
unsigned long CmdO2EnvSize(void);
CmdO2Rc CmdO2EnvExport(char *buffer, unsigned long cap, unsigned long *actual);
CmdO2Rc CmdO2EnvImport(const char *block);

int CmdO2PathExists(const char *path);
int CmdO2IsDirectory(const char *path);

#endif
