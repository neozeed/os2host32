/*
 * cmdos2_win32.c - native bootstrap backend for CMD's OS/2 service boundary.
 *
 * Deliberately contains the Win32 DLL-loader knowledge that CMD itself is
 * being taught to forget.  Filesystem/process operations are resolved by the
 * historical 32-bit DOSCALLS ordinals and executed in DOSCALLS.dll.
 *
 * ANSI C89-oriented; intended for old MSVC as well as MinGW.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "cmdos2.h"
#include "cmdos2_env.h"

#ifndef __cdecl
#define __cdecl
#endif

#define O2_FIL_STANDARD 1UL
#define O2_FIND_BUF 320UL
#define O2_FIND_ATTRS (CMDO2_ATTR_READONLY | CMDO2_ATTR_HIDDEN | \
                       CMDO2_ATTR_SYSTEM | CMDO2_ATTR_DIRECTORY | \
                       CMDO2_ATTR_ARCHIVED)

#define O2_OPEN_ACTION_OPEN_IF_EXISTS     0x0001UL
#define O2_OPEN_ACTION_REPLACE_IF_EXISTS  0x0002UL
#define O2_OPEN_ACTION_CREATE_IF_NEW      0x0010UL
#define O2_OPEN_ACCESS_READONLY           0x0000UL
#define O2_OPEN_ACCESS_WRITEONLY          0x0001UL
#define O2_OPEN_SHARE_DENYNONE            0x0040UL

struct O2ResultCodes {
    unsigned long codeTerminate;
    unsigned long codeResult;
};

struct O2VioModeInfo {
    unsigned short cb;
    unsigned char fbType;
    unsigned char color;
    unsigned short col;
    unsigned short row;
    unsigned short hres;
    unsigned short vres;
    unsigned char fmt_ID;
    unsigned char attrib;
    unsigned long buf_addr;
    unsigned long buf_length;
    unsigned long full_length;
    unsigned long partial_length;
    char *ext_data_addr;
};

#pragma pack(2)
struct O2KbdKeyInfo {
    unsigned char chChar;
    unsigned char chScan;
    unsigned char fbStatus;
    unsigned char bNlsShift;
    unsigned short fsState;
    unsigned long time;
};
#pragma pack()
typedef char O2KbdKeyInfo_must_be_10_bytes[(sizeof(struct O2KbdKeyInfo) == 10) ? 1 : -1];

typedef CmdO2Rc (__cdecl *PFN_SETDEFAULTDISK)(unsigned long);
typedef CmdO2Rc (__cdecl *PFN_CREATEPIPE)(CmdO2Handle *, CmdO2Handle *,
                                           unsigned long);
typedef CmdO2Rc (__cdecl *PFN_DUPHANDLE)(CmdO2Handle, CmdO2Handle *);
typedef CmdO2Rc (__cdecl *PFN_SETFILEPTR)(CmdO2Handle, long,
                                            unsigned long, unsigned long *);
typedef CmdO2Rc (__cdecl *PFN_EXEC)(char *, long, unsigned long,
                                     const char *, const char *,
                                     struct O2ResultCodes *, const char *);
typedef CmdO2Rc (__cdecl *PFN_WAITCHILD)(unsigned long, unsigned long,
                                          struct O2ResultCodes *,
                                          unsigned long *, unsigned long);
typedef CmdO2Rc (__cdecl *PFN_QAPPTYPE)(const char *, unsigned long *);
typedef CmdO2Rc (__cdecl *PFN_SETCURDIR)(const char *);
typedef CmdO2Rc (__cdecl *PFN_CLOSE)(CmdO2Handle);
typedef CmdO2Rc (__cdecl *PFN_DELETE)(const char *, unsigned long);
typedef CmdO2Rc (__cdecl *PFN_FINDCLOSE)(CmdO2FindHandle);
typedef CmdO2Rc (__cdecl *PFN_FINDFIRST)(const char *, CmdO2FindHandle *,
                                          unsigned long, void *, unsigned long,
                                          unsigned long *, unsigned long);
typedef CmdO2Rc (__cdecl *PFN_FINDNEXT)(CmdO2FindHandle, void *,
                                         unsigned long, unsigned long *);
typedef CmdO2Rc (__cdecl *PFN_CREATEDIR)(const char *, void *, unsigned long);
typedef CmdO2Rc (__cdecl *PFN_MOVE)(const char *, const char *);
typedef CmdO2Rc (__cdecl *PFN_OPEN)(const char *, CmdO2Handle *,
                                     unsigned long *, unsigned long,
                                     unsigned long, unsigned long,
                                     unsigned long, void *, unsigned long);
typedef CmdO2Rc (__cdecl *PFN_QUERYCURDIR)(unsigned long, char *,
                                            unsigned long *);
typedef CmdO2Rc (__cdecl *PFN_QUERYCURDISK)(unsigned long *, unsigned long *);
typedef CmdO2Rc (__cdecl *PFN_QUERYPATH)(const char *, unsigned long,
                                          void *, unsigned long);
typedef CmdO2Rc (__cdecl *PFN_READ)(CmdO2Handle, void *, unsigned long,
                                     unsigned long *);
typedef CmdO2Rc (__cdecl *PFN_WRITE)(CmdO2Handle, const void *, unsigned long,
                                      unsigned long *);
typedef CmdO2Rc (__cdecl *PFN_DELETEDIR)(const char *);

typedef unsigned short (__cdecl *PFN_VIOWRTTTY)(const char *, unsigned short,
                                                  unsigned short);
typedef unsigned short (__cdecl *PFN_VIOGETCURPOS)(unsigned short *,
                                                     unsigned short *,
                                                     unsigned short);
typedef unsigned short (__cdecl *PFN_VIOSETCURPOS)(unsigned short,
                                                     unsigned short,
                                                     unsigned short);
typedef unsigned short (__cdecl *PFN_VIOGETMODE)(struct O2VioModeInfo *,
                                                  unsigned short);
typedef unsigned short (__cdecl *PFN_VIOSCROLLUP)(unsigned short, unsigned short,
                                                   unsigned short, unsigned short,
                                                   unsigned short,
                                                   const unsigned char *,
                                                   unsigned short);
typedef unsigned short (__cdecl *PFN_KBDCHARIN)(struct O2KbdKeyInfo *,
                                                 unsigned short, unsigned short);
typedef unsigned short (__cdecl *PFN_KBDFLUSH)(unsigned short);

struct CmdO2Api {
    HMODULE module;
    HMODULE vio_module;
    HMODULE kbd_module;
    PFN_SETDEFAULTDISK setDefaultDisk;
    PFN_CREATEPIPE createPipe;
    PFN_DUPHANDLE dupHandle;
    PFN_SETFILEPTR setFilePtr;
    PFN_EXEC execPgm;
    PFN_WAITCHILD waitChild;
    PFN_QAPPTYPE queryAppType;
    PFN_SETCURDIR setCurrentDir;
    PFN_CLOSE close;
    PFN_DELETE deleteFile;
    PFN_FINDCLOSE findClose;
    PFN_FINDFIRST findFirst;
    PFN_FINDNEXT findNext;
    PFN_CREATEDIR createDir;
    PFN_MOVE move;
    PFN_OPEN open;
    PFN_QUERYCURDIR queryCurrentDir;
    PFN_QUERYCURDISK queryCurrentDisk;
    PFN_QUERYPATH queryPathInfo;
    PFN_READ read;
    PFN_WRITE write;
    PFN_DELETEDIR deleteDir;
    PFN_VIOWRTTTY vioWrtTTY;
    PFN_VIOGETCURPOS vioGetCurPos;
    PFN_VIOSETCURPOS vioSetCurPos;
    PFN_VIOGETMODE vioGetMode;
    PFN_VIOSCROLLUP vioScrollUp;
    PFN_KBDCHARIN kbdCharIn;
    PFN_KBDFLUSH kbdFlushBuffer;
};

static struct CmdO2Api api;
static char init_error[160];

static CmdO2Rc native_mirror_std_to_crt(unsigned long stdHandle);
static int native_sync_inherited_std(void);

static FARPROC ordinal_proc(HMODULE module, unsigned ordinal)
{
    return GetProcAddress(module, MAKEINTRESOURCEA(ordinal));
}

#define LOAD_ORD_FROM(modfield, modname, field, type, ord) do { \
    FARPROC _p = ordinal_proc(api.modfield, (ord)); \
    union { FARPROC p; type f; } _u; \
    if (_p == NULL) { \
        sprintf(init_error, modname " ordinal %u is missing", (unsigned)(ord)); \
        goto fail; \
    } \
    _u.p = _p; \
    api.field = _u.f; \
} while (0)

#define LOAD_ORD(field, type, ord) \
    LOAD_ORD_FROM(module, "DOSCALLS", field, type, ord)


int CmdO2PrepareLoader(void)
{
    char path[MAX_PATH];
    char *slash;
    char *slash2;
    char *last;
    DWORD n;
    DWORD attrs;
    size_t need;

    n = GetEnvironmentVariableA("OS2HOST32_LOADER", path,
                                (DWORD)sizeof(path));
    if (n != 0 && n < (DWORD)sizeof(path))
        return 1;

    n = GetModuleFileNameA(NULL, path, (DWORD)sizeof(path));
    if (n == 0 || n >= (DWORD)sizeof(path))
        return 0;
    slash = strrchr(path, '\\');
    slash2 = strrchr(path, '/');
    last = slash;
    if (slash2 != NULL && (last == NULL || slash2 > last))
        last = slash2;
    if (last != NULL)
        last[1] = '\0';
    else
        path[0] = '\0';
    need = strlen(path) + strlen("os2host32.exe") + 1U;
    if (need > sizeof(path))
        return 0;
    strcat(path, "os2host32.exe");
    attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES ||
        (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0)
        return 0;
    return SetEnvironmentVariableA("OS2HOST32_LOADER", path) ? 1 : 0;
}

int CmdO2Init(void)
{
    memset(&api, 0, sizeof(api));
    init_error[0] = '\0';
    api.module = LoadLibraryA("DOSCALLS.dll");
    if (api.module == NULL) {
        sprintf(init_error, "cannot load DOSCALLS.dll (host error %lu)",
                (unsigned long)GetLastError());
        return 0;
    }
    api.vio_module = LoadLibraryA("VIOCALLS.dll");
    if (api.vio_module == NULL) {
        sprintf(init_error, "cannot load VIOCALLS.dll (host error %lu)",
                (unsigned long)GetLastError());
        goto fail;
    }
    api.kbd_module = LoadLibraryA("KBDCALLS.dll");
    if (api.kbd_module == NULL) {
        sprintf(init_error, "cannot load KBDCALLS.dll (host error %lu)",
                (unsigned long)GetLastError());
        goto fail;
    }

    {
        LPCH env;
        env = GetEnvironmentStringsA();
        if (env == NULL || !CmdEnvStoreInit(env)) {
            if (env != NULL)
                FreeEnvironmentStringsA(env);
            sprintf(init_error, "cannot snapshot initial process environment");
            goto fail;
        }
        FreeEnvironmentStringsA(env);
    }
    if (!CmdEnvStoreApplyOs2Path()) {
        sprintf(init_error, "cannot initialize OS/2 PATH namespace");
        goto fail;
    }

    LOAD_ORD(queryAppType, PFN_QAPPTYPE, 163);
    LOAD_ORD(setDefaultDisk, PFN_SETDEFAULTDISK, 220);
    LOAD_ORD(createPipe, PFN_CREATEPIPE, 239);
    LOAD_ORD(deleteDir, PFN_DELETEDIR, 226);
    LOAD_ORD(setCurrentDir, PFN_SETCURDIR, 255);
    LOAD_ORD(setFilePtr, PFN_SETFILEPTR, 256);
    LOAD_ORD(close, PFN_CLOSE, 257);
    LOAD_ORD(deleteFile, PFN_DELETE, 259);
    LOAD_ORD(dupHandle, PFN_DUPHANDLE, 260);
    LOAD_ORD(findClose, PFN_FINDCLOSE, 263);
    LOAD_ORD(findFirst, PFN_FINDFIRST, 264);
    LOAD_ORD(findNext, PFN_FINDNEXT, 265);
    LOAD_ORD(createDir, PFN_CREATEDIR, 270);
    LOAD_ORD(move, PFN_MOVE, 271);
    LOAD_ORD(open, PFN_OPEN, 273);
    LOAD_ORD(queryCurrentDir, PFN_QUERYCURDIR, 274);
    LOAD_ORD(queryCurrentDisk, PFN_QUERYCURDISK, 275);
    LOAD_ORD(queryPathInfo, PFN_QUERYPATH, 223);
    LOAD_ORD(read, PFN_READ, 281);
    LOAD_ORD(waitChild, PFN_WAITCHILD, 280);
    LOAD_ORD(write, PFN_WRITE, 282);
    LOAD_ORD(execPgm, PFN_EXEC, 283);

    LOAD_ORD_FROM(vio_module, "VIOCALLS", vioScrollUp, PFN_VIOSCROLLUP, 7);
    LOAD_ORD_FROM(vio_module, "VIOCALLS", vioGetCurPos, PFN_VIOGETCURPOS, 9);
    LOAD_ORD_FROM(vio_module, "VIOCALLS", vioSetCurPos, PFN_VIOSETCURPOS, 15);
    LOAD_ORD_FROM(vio_module, "VIOCALLS", vioWrtTTY, PFN_VIOWRTTTY, 19);
    LOAD_ORD_FROM(vio_module, "VIOCALLS", vioGetMode, PFN_VIOGETMODE, 21);
    LOAD_ORD_FROM(kbd_module, "KBDCALLS", kbdCharIn, PFN_KBDCHARIN, 4);
    LOAD_ORD_FROM(kbd_module, "KBDCALLS", kbdFlushBuffer, PFN_KBDFLUSH, 13);

    /* A pipeline worker is a fresh native Win32 process.  STARTUPINFO gives
     * it the correct inherited Win32 standard HANDLEs, but MinGW/MSVCRT does
     * not reliably bind fd 0/1/2 to those HANDLEs for our bootstrap shell.
     * Normalize the CRT descriptors immediately so builtins and subsequent
     * DosExecPgm calls observe the same standard streams. */
    if (!native_sync_inherited_std()) {
        sprintf(init_error, "cannot synchronize inherited standard handles");
        goto fail;
    }
    return 1;

fail:
    CmdEnvStoreDone();
    if (api.kbd_module != NULL)
        FreeLibrary(api.kbd_module);
    if (api.vio_module != NULL)
        FreeLibrary(api.vio_module);
    if (api.module != NULL)
        FreeLibrary(api.module);
    memset(&api, 0, sizeof(api));
    return 0;
}

void CmdO2Done(void)
{
    CmdEnvStoreDone();
    if (api.kbd_module != NULL)
        FreeLibrary(api.kbd_module);
    if (api.vio_module != NULL)
        FreeLibrary(api.vio_module);
    if (api.module != NULL)
        FreeLibrary(api.module);
    memset(&api, 0, sizeof(api));
}

const char *CmdO2InitError(void)
{
    return init_error;
}

static unsigned short get16(const unsigned char *p)
{
    return (unsigned short)((unsigned)p[0] | ((unsigned)p[1] << 8));
}

static unsigned long get32(const unsigned char *p)
{
    return (unsigned long)p[0] |
           ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) |
           ((unsigned long)p[3] << 24);
}

static void decode_find(const unsigned char *b, struct CmdO2FindData *d)
{
    unsigned n;
    memset(d, 0, sizeof(*d));
    /* FILEFINDBUF3: ULONG oNextEntryOffset precedes the timestamps. */
    d->date = get16(b + 12);
    d->time = get16(b + 14);
    d->size = get32(b + 16);
    d->alloc = get32(b + 20);
    d->attr = get32(b + 24);
    n = (unsigned)b[28];
    if (n >= sizeof(d->name))
        n = (unsigned)sizeof(d->name) - 1U;
    memcpy(d->name, b + 29, n);
    d->name[n] = '\0';
}

CmdO2Rc CmdO2ExecPgm(char *objectName, long objectNameLen,
                     unsigned long execFlag, const char *args,
                     const char *env, struct CmdO2ResultCodes *results,
                     const char *program)
{
    struct O2ResultCodes r;
    CmdO2Rc rc;
    if (api.execPgm == NULL || results == NULL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    r.codeTerminate = 0;
    r.codeResult = 0;
    if (env == NULL)
        env = CmdEnvStoreBlock();
    rc = api.execPgm(objectName, objectNameLen, execFlag, args, env, &r, program);
    results->codeTerminate = r.codeTerminate;
    results->codeResult = r.codeResult;
    return rc;
}

CmdO2Rc CmdO2QueryAppType(const char *program, unsigned long *appType)
{
    if (api.queryAppType == NULL || program == NULL || appType == NULL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    return api.queryAppType(program, appType);
}

CmdO2Rc CmdO2WaitChild(unsigned long action, unsigned long option,
                       struct CmdO2ResultCodes *results,
                       unsigned long *pid, unsigned long childPid)
{
    struct O2ResultCodes r;
    CmdO2Rc rc;

    if (api.waitChild == NULL || results == NULL || pid == NULL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    r.codeTerminate = 0;
    r.codeResult = 0;
    *pid = 0;
    rc = api.waitChild(action, option, &r, pid, childPid);
    results->codeTerminate = r.codeTerminate;
    results->codeResult = r.codeResult;
    return rc;
}

CmdO2Rc CmdO2QueryProgramPath(char *buffer, unsigned long cap)
{
    DWORD n;

    if (buffer == NULL || cap == 0UL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    n = GetModuleFileNameA(NULL, buffer, (DWORD)cap);
    if (n == 0)
        return (CmdO2Rc)GetLastError();
    if (n >= (DWORD)cap) {
        buffer[cap - 1UL] = '\0';
        return CMDO2_ERROR_BUFFER_OVERFLOW;
    }
    return CMDO2_NO_ERROR;
}

CmdO2Rc CmdO2OpenRead(const char *path, CmdO2Handle *handle)
{
    unsigned long action;
    action = 0;
    return api.open(path, handle, &action, 0, 0,
                    O2_OPEN_ACTION_OPEN_IF_EXISTS,
                    O2_OPEN_ACCESS_READONLY | O2_OPEN_SHARE_DENYNONE,
                    NULL, 0);
}

CmdO2Rc CmdO2OpenWriteReplace(const char *path, CmdO2Handle *handle)
{
    unsigned long action;
    action = 0;
    return api.open(path, handle, &action, 0, 0,
                    O2_OPEN_ACTION_REPLACE_IF_EXISTS |
                    O2_OPEN_ACTION_CREATE_IF_NEW,
                    O2_OPEN_ACCESS_WRITEONLY | O2_OPEN_SHARE_DENYNONE,
                    NULL, 0);
}

CmdO2Rc CmdO2Read(CmdO2Handle handle, void *buffer,
                  unsigned long count, unsigned long *actual)
{
    return api.read(handle, buffer, count, actual);
}

CmdO2Rc CmdO2Write(CmdO2Handle handle, const void *buffer,
                   unsigned long count, unsigned long *actual)
{
    return api.write(handle, buffer, count, actual);
}

CmdO2Rc CmdO2Close(CmdO2Handle handle)
{
    return api.close(handle);
}

CmdO2Rc CmdO2CreatePipe(CmdO2Handle *readHandle, CmdO2Handle *writeHandle,
                        unsigned long size)
{
    return api.createPipe(readHandle, writeHandle, size);
}

CmdO2Rc CmdO2DupHandle(CmdO2Handle oldHandle, CmdO2Handle *newHandle)
{
    return api.dupHandle(oldHandle, newHandle);
}

static DWORD native_std_id(unsigned long stdHandle)
{
    if (stdHandle == 0UL)
        return STD_INPUT_HANDLE;
    if (stdHandle == 1UL)
        return STD_OUTPUT_HANDLE;
    return STD_ERROR_HANDLE;
}

static int native_std_fd(unsigned long stdHandle)
{
    if (stdHandle > 2UL)
        return -1;
    return (int)stdHandle;
}

CmdO2Rc CmdO2StdSave(unsigned long stdHandle, struct CmdO2StdToken *token)
{
    CmdO2Handle saved;
    CmdO2Rc rc;

    if (token == NULL || stdHandle > 2UL)
        return CMDO2_ERROR_INVALID_PARAMETER;

    token->value = CMDO2_HANDLE_ALLOCATE;
    token->os2_value = CMDO2_HANDLE_ALLOCATE;

    /* The OS/2 handle table is authoritative.  Save one duplicate there;
     * the CRT side can always be reconstructed from the restored HFILE. */
    saved = CMDO2_HANDLE_ALLOCATE;
    rc = api.dupHandle((CmdO2Handle)stdHandle, &saved);
    if (rc != 0)
        return rc;

    token->os2_value = (unsigned long)saved;
    return CMDO2_NO_ERROR;
}

static CmdO2Rc native_mirror_std_to_crt(unsigned long stdHandle)
{
    HANDLE h;
    HANDLE copy;
    HANDLE proc;
    int tfd;
    int fd;

    fd = native_std_fd(stdHandle);
    if (fd < 0)
        return CMDO2_ERROR_INVALID_PARAMETER;

    h = GetStdHandle(native_std_id(stdHandle));
    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return CMDO2_ERROR_INVALID_HANDLE;

    proc = GetCurrentProcess();
    copy = INVALID_HANDLE_VALUE;
    if (!DuplicateHandle(proc, h, proc, &copy, 0, TRUE,
                         DUPLICATE_SAME_ACCESS))
        return (CmdO2Rc)GetLastError();

    tfd = _open_osfhandle((long)copy, _O_TEXT);
    if (tfd < 0) {
        CloseHandle(copy);
        return CMDO2_ERROR_INVALID_HANDLE;
    }
    if (_dup2(tfd, fd) != 0) {
        _close(tfd);
        return CMDO2_ERROR_INVALID_HANDLE;
    }
    _close(tfd);

    /* _dup2() is allowed to replace the CRT descriptor with a different
     * native HANDLE.  Re-publish the HANDLE that actually belongs to fd
     * 0/1/2 after closing the temporary descriptor.  This is the important
     * distinction between the DOSCALLS-owned duplicate and the CRT-owned
     * duplicate: CreateProcess must inherit the latter, not a stale HANDLE
     * value that happened to be current before _dup2(). */
    h = (HANDLE)(long)_get_osfhandle(fd);
    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return CMDO2_ERROR_INVALID_HANDLE;
    (void)SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    if (!SetStdHandle(native_std_id(stdHandle), h))
        return (CmdO2Rc)GetLastError();

    if (fd == 0)
        clearerr(stdin);
    else if (fd == 1)
        clearerr(stdout);
    else
        clearerr(stderr);
    return CMDO2_NO_ERROR;
}

static int native_sync_inherited_std(void)
{
    unsigned long i;
    HANDLE h;
    CmdO2Rc rc;

    for (i = 0UL; i < 3UL; ++i) {
        h = GetStdHandle(native_std_id(i));
        if (h == NULL || h == INVALID_HANDLE_VALUE)
            continue;
        rc = native_mirror_std_to_crt(i);
        if (rc != 0)
            return 0;
    }
    return 1;
}

CmdO2Rc CmdO2StdRedirectPath(unsigned long stdHandle, const char *path,
                             int redirType)
{
    CmdO2Handle h;
    unsigned long action;
    unsigned long newpos;
    unsigned long openAction;
    unsigned long openMode;
    CmdO2Rc rc;

    if (path == NULL || stdHandle > 2UL)
        return CMDO2_ERROR_INVALID_PARAMETER;

    h = 0;
    action = 0;
    if (redirType == CMDO2_REDIR_INPUT) {
        if (stdHandle != 0UL)
            return CMDO2_ERROR_INVALID_PARAMETER;
        openAction = O2_OPEN_ACTION_OPEN_IF_EXISTS;
        openMode = O2_OPEN_ACCESS_READONLY | O2_OPEN_SHARE_DENYNONE;
    } else if (redirType == CMDO2_REDIR_OUTPUT) {
        if (stdHandle != 1UL && stdHandle != 2UL)
            return CMDO2_ERROR_INVALID_PARAMETER;
        openAction = O2_OPEN_ACTION_REPLACE_IF_EXISTS |
                     O2_OPEN_ACTION_CREATE_IF_NEW;
        openMode = O2_OPEN_ACCESS_WRITEONLY | O2_OPEN_SHARE_DENYNONE;
    } else if (redirType == CMDO2_REDIR_APPEND) {
        if (stdHandle != 1UL && stdHandle != 2UL)
            return CMDO2_ERROR_INVALID_PARAMETER;
        openAction = O2_OPEN_ACTION_OPEN_IF_EXISTS |
                     O2_OPEN_ACTION_CREATE_IF_NEW;
        openMode = O2_OPEN_ACCESS_WRITEONLY | O2_OPEN_SHARE_DENYNONE;
    } else {
        return CMDO2_ERROR_INVALID_PARAMETER;
    }

    rc = api.open(path, &h, &action, 0UL, 0UL, openAction,
                  openMode, NULL, 0UL);
    if (rc != 0)
        return rc;

    if (redirType == CMDO2_REDIR_APPEND) {
        newpos = 0;
        rc = api.setFilePtr(h, 0L, 2UL, &newpos);
        if (rc != 0) {
            (void)api.close(h);
            return rc;
        }
    }

    rc = CmdO2StdRedirectHandle(stdHandle, h);
    (void)api.close(h);
    return rc;
}

CmdO2Rc CmdO2StdRedirectHandle(unsigned long stdHandle, CmdO2Handle source)
{
    CmdO2Handle target;
    CmdO2Rc rc;

    if (stdHandle > 2UL)
        return CMDO2_ERROR_INVALID_PARAMETER;

    target = (CmdO2Handle)stdHandle;
    rc = api.dupHandle(source, &target);
    if (rc != 0)
        return rc;

    /* DOSCALLS owns the authoritative duplicate; the native CRT gets a
     * separate copy.  This is only bootstrap glue and disappears once CMD is
     * itself an LX process. */
    return native_mirror_std_to_crt(stdHandle);
}

CmdO2Rc CmdO2StdRestore(unsigned long stdHandle, struct CmdO2StdToken *token)
{
    CmdO2Handle target;
    CmdO2Handle saved;
    CmdO2Rc rc;

    if (token == NULL || stdHandle > 2UL ||
        token->os2_value == CMDO2_HANDLE_ALLOCATE)
        return CMDO2_ERROR_INVALID_PARAMETER;

    fflush(stdout);
    fflush(stderr);

    saved = (CmdO2Handle)token->os2_value;
    target = (CmdO2Handle)stdHandle;
    rc = api.dupHandle(saved, &target);
    (void)api.close(saved);
    token->os2_value = CMDO2_HANDLE_ALLOCATE;
    token->value = CMDO2_HANDLE_ALLOCATE;
    if (rc != 0)
        return rc;

    return native_mirror_std_to_crt(stdHandle);
}

CmdO2Rc CmdO2VioWrtTTY(const char *text, unsigned long count)
{
    unsigned short rc;
    unsigned long off;
    unsigned short chunk;
    if (api.vioWrtTTY == NULL || (text == NULL && count != 0UL))
        return CMDO2_ERROR_INVALID_PARAMETER;
    off = 0UL;
    while (off < count) {
        unsigned long left;
        left = count - off;
        chunk = (unsigned short)(left > 65535UL ? 65535UL : left);
        rc = api.vioWrtTTY(text + off, chunk, 0U);
        if (rc != 0U)
            return (CmdO2Rc)rc;
        off += (unsigned long)chunk;
    }
    return CMDO2_NO_ERROR;
}

CmdO2Rc CmdO2VioGetCurPos(unsigned short *row, unsigned short *col)
{
    if (api.vioGetCurPos == NULL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    return (CmdO2Rc)api.vioGetCurPos(row, col, 0U);
}

CmdO2Rc CmdO2VioSetCurPos(unsigned short row, unsigned short col)
{
    if (api.vioSetCurPos == NULL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    return (CmdO2Rc)api.vioSetCurPos(row, col, 0U);
}

CmdO2Rc CmdO2VioGetScreenSize(unsigned short *rows, unsigned short *cols)
{
    struct O2VioModeInfo mode;
    unsigned short rc;

    if (api.vioGetMode == NULL || rows == NULL || cols == NULL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    memset(&mode, 0, sizeof(mode));
    mode.cb = (unsigned short)sizeof(mode);
    rc = api.vioGetMode(&mode, 0U);
    if (rc != 0U)
        return (CmdO2Rc)rc;
    *rows = mode.row;
    *cols = mode.col;
    return CMDO2_NO_ERROR;
}

CmdO2Rc CmdO2VioClear(void)
{
    struct O2VioModeInfo mode;
    unsigned char cell[2];
    unsigned short rc;
    if (api.vioGetMode == NULL || api.vioScrollUp == NULL ||
        api.vioSetCurPos == NULL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    memset(&mode, 0, sizeof(mode));
    mode.cb = (unsigned short)sizeof(mode);
    rc = api.vioGetMode(&mode, 0U);
    if (rc != 0U)
        return (CmdO2Rc)rc;
    cell[0] = ' ';
    cell[1] = 7U;
    rc = api.vioScrollUp(0U, 0U,
                         mode.row != 0U ? (unsigned short)(mode.row - 1U) : 0U,
                         mode.col != 0U ? (unsigned short)(mode.col - 1U) : 0U,
                         0xffffU, cell, 0U);
    if (rc != 0U)
        return (CmdO2Rc)rc;
    return (CmdO2Rc)api.vioSetCurPos(0U, 0U, 0U);
}

CmdO2Rc CmdO2KbdCharIn(struct CmdO2KbdKeyInfo *info, unsigned long wait)
{
    struct O2KbdKeyInfo k;
    unsigned short rc;
    if (api.kbdCharIn == NULL || info == NULL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    memset(&k, 0, sizeof(k));
    rc = api.kbdCharIn(&k,
                       wait == CMDO2_IO_NOWAIT ? 1U : 0U,
                       0U);
    if (rc != 0U)
        return (CmdO2Rc)rc;
    info->chChar = k.chChar;
    info->chScan = k.chScan;
    info->fbStatus = k.fbStatus;
    info->bNlsShift = k.bNlsShift;
    info->fsState = k.fsState;
    info->time = k.time;
    return CMDO2_NO_ERROR;
}

CmdO2Rc CmdO2KbdFlushBuffer(void)
{
    if (api.kbdFlushBuffer == NULL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    return (CmdO2Rc)api.kbdFlushBuffer(0U);
}

CmdO2Rc CmdO2Delete(const char *path)
{
    return api.deleteFile(path, 0);
}

CmdO2Rc CmdO2Move(const char *oldPath, const char *newPath)
{
    return api.move(oldPath, newPath);
}

CmdO2Rc CmdO2CreateDir(const char *path)
{
    return api.createDir(path, NULL, 0);
}

CmdO2Rc CmdO2DeleteDir(const char *path)
{
    return api.deleteDir(path);
}

CmdO2Rc CmdO2SetCurrentDir(const char *path)
{
    return api.setCurrentDir(path);
}

CmdO2Rc CmdO2SetDefaultDisk(unsigned long disk)
{
    if (api.setDefaultDisk == NULL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    return api.setDefaultDisk(disk);
}

CmdO2Rc CmdO2QueryCurrentDir(char *buffer, unsigned long cap)
{
    unsigned long disk;
    unsigned long map;
    unsigned long len;
    char sub[1024];
    CmdO2Rc rc;
    unsigned char letter;
    size_t n;
    const char *part;

    if (buffer == NULL || cap < 4UL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    disk = 0;
    map = 0;
    rc = api.queryCurrentDisk(&disk, &map);
    if (rc != 0)
        return rc;
    (void)map;
    len = (unsigned long)sizeof(sub);
    rc = api.queryCurrentDir(0, sub, &len);
    if (rc != 0)
        return rc;
    if (disk < 1UL || disk > 26UL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    letter = (unsigned char)('A' + (unsigned)(disk - 1UL));
    part = sub;
    if (*part == '\\' || *part == '/')
        ++part;
    n = strlen(part);
    if ((unsigned long)n + 4UL > cap)
        return CMDO2_ERROR_INVALID_PARAMETER;
    buffer[0] = (char)letter;
    buffer[1] = ':';
    buffer[2] = '\\';
    strcpy(buffer + 3, part);
    return CMDO2_NO_ERROR;
}

CmdO2Rc CmdO2QueryPathAttr(const char *path, unsigned long *attr)
{
    unsigned char b[24];
    CmdO2Rc rc;
    if (attr == NULL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    memset(b, 0, sizeof(b));
    rc = api.queryPathInfo(path, O2_FIL_STANDARD, b, (unsigned long)sizeof(b));
    if (rc != 0)
        return rc;
    *attr = get32(b + 20);
    return 0;
}

CmdO2Rc CmdO2FindFirst(const char *pattern, CmdO2FindHandle *handle,
                       struct CmdO2FindData *data)
{
    unsigned char b[O2_FIND_BUF];
    unsigned long count;
    CmdO2Rc rc;
    if (handle == NULL || data == NULL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    memset(b, 0, sizeof(b));
    *handle = CMDO2_FIND_CREATE;
    count = 1;
    rc = api.findFirst(pattern, handle, O2_FIND_ATTRS, b,
                       (unsigned long)sizeof(b), &count, O2_FIL_STANDARD);
    if (rc != 0)
        return rc;
    if (count == 0)
        return CMDO2_ERROR_NO_MORE_FILES;
    decode_find(b, data);
    return 0;
}

CmdO2Rc CmdO2FindNext(CmdO2FindHandle handle, struct CmdO2FindData *data)
{
    unsigned char b[O2_FIND_BUF];
    unsigned long count;
    CmdO2Rc rc;
    if (data == NULL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    memset(b, 0, sizeof(b));
    count = 1;
    rc = api.findNext(handle, b, (unsigned long)sizeof(b), &count);
    if (rc != 0)
        return rc;
    if (count == 0)
        return CMDO2_ERROR_NO_MORE_FILES;
    decode_find(b, data);
    return 0;
}

CmdO2Rc CmdO2FindClose(CmdO2FindHandle handle)
{
    return api.findClose(handle);
}

int CmdO2PathExists(const char *path)
{
    unsigned long attr;
    return CmdO2QueryPathAttr(path, &attr) == 0;
}

int CmdO2IsDirectory(const char *path)
{
    unsigned long attr;
    if (CmdO2QueryPathAttr(path, &attr) != 0)
        return 0;
    return (attr & CMDO2_ATTR_DIRECTORY) != 0;
}
