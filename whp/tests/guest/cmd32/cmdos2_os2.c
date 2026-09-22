/*
 * cmdos2_os2.c - direct OS/2 2.x / Microsoft C/386 backend for cmdos2.h.
 *
 * M28B direct-OS/2 path:
 *   This is an OS/2 client.  Do not redeclare the Dos* API locally.
 *   Pull the ABI, types, calling conventions and structures from os2.h and
 *   let OS2386.LIB provide the imports at link time.
 *
 * The OS/2 declarations in os2.h provide the actual API ABI.  For this smoke
 * path we deliberately avoid ordinary CRT string/memory entry points because
 * the recovered compiler/header combination currently decorates them
 * differently from this OS/2 LIBC.LIB.
 */

#define INCL_DOSFILEMGR
#define INCL_DOSPROCESS
#define INCL_DOSQUEUES
#define INCL_DOSSESMGR
#define INCL_DOSERRORS
#include <os2.h>

#include "cmdos2.h"
#include "cmdos2_env.h"

/*
 * Microsoft C 6.00 startup state.
 *
 * This prerelease C/386 SDK does not import the later DosGetInfoBlocks API.
 * The matching Microsoft C runtime already publishes the process environment
 * as environ[] and the fully-qualified executable path as _pgmptr.  Using
 * those startup globals also keeps the direct backend on the exact startup
 * contract OS2HOST32 already reconstructs for original C/386 LX programs.
 */
extern char *environ[];
extern char *_pgmptr;

static char os2_initial_env[CMDO2_ENV_MAX];

/*
 * Do not depend on the SDK typedef name FILEFINDBUF3 here.  Some early
 * OS/2 2.0 prerelease C/386 headers predate that public typedef.  The later
 * 32-bit DosFindFirst/DosFindNext FIL_STANDARD ABI (and our DOSCALLS shim)
 * use the layout below.  Keeping the wire layout private lets this source at
 * least compile against both header generations; the Beta 2 runtime smoke
 * test remains the authority for whether that prerelease ABI is identical.
 */
struct CmdFileFindBuf3 {
    ULONG oNextEntryOffset;
    FDATE fdateCreation;
    FTIME ftimeCreation;
    FDATE fdateLastAccess;
    FTIME ftimeLastAccess;
    FDATE fdateLastWrite;
    FTIME ftimeLastWrite;
    ULONG cbFile;
    ULONG cbFileAlloc;
    ULONG attrFile;
    UCHAR cchName;
    CHAR achName[260];
};

struct CmdFileStatus3 {
    FDATE fdateCreation;
    FTIME ftimeCreation;
    FDATE fdateLastAccess;
    FTIME ftimeLastAccess;
    FDATE fdateLastWrite;
    FTIME ftimeLastWrite;
    ULONG cbFile;
    ULONG cbFileAlloc;
    ULONG attrFile;
};

/*
 * Keep the direct OS/2 backend independent of the mismatched CRT calling-
 * convention decoration currently emitted by the recovered C/386 driver.
 * The OS/2 LIBC import library exports cdecl names (_strlen, _memcpy, ...),
 * while this compiler/header combination currently emits _strlen@4,
 * _memcpy@12, etc.  These tiny private helpers let us prove the DOSCALLS ABI
 * first without papering over that separate toolchain issue.
 */
static unsigned cmd_strlen(const char *s)
{
    const char *p;
    p = s;
    while (*p != '\0')
        ++p;
    return (unsigned)(p - s);
}

static void cmd_memcpy(void *vd, const void *vs, unsigned n)
{
    unsigned char *d;
    const unsigned char *s;
    d = (unsigned char *)vd;
    s = (const unsigned char *)vs;
    while (n-- != 0U)
        *d++ = *s++;
}

static void cmd_memset(void *vd, int c, unsigned n)
{
    unsigned char *d;
    d = (unsigned char *)vd;
    while (n-- != 0U)
        *d++ = (unsigned char)c;
}

static void cmd_strcpy(char *d, const char *s)
{
    do {
        *d++ = *s;
    } while (*s++ != '\0');
}

#define O2_FIND_ATTRS (FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM | \
                       FILE_DIRECTORY | FILE_ARCHIVED)

static const char *os2_init_error = "";

int CmdO2PrepareLoader(void) { return 1; }

int CmdO2Init(void)
{
    unsigned long used;
    unsigned long n;
    unsigned i;

    os2_init_error = "";
    used = 0UL;
    i = 0U;
    while (environ[i] != (char *)0) {
        n = (unsigned long)cmd_strlen(environ[i]) + 1UL;
        if (used + n + 1UL > CMDO2_ENV_MAX) {
            os2_init_error = "initial environment is too large";
            return 0;
        }
        cmd_memcpy(os2_initial_env + used, environ[i], (unsigned)n);
        used += n;
        ++i;
    }
    os2_initial_env[used++] = '\0';
    if (used == 1UL)
        os2_initial_env[used++] = '\0';

    if (!CmdEnvStoreInit(os2_initial_env)) {
        os2_init_error = "initial environment is too large";
        return 0;
    }
    if (!CmdEnvStoreApplyOs2Path()) {
        os2_init_error = "cannot initialize OS/2 PATH namespace";
        return 0;
    }
    return 1;
}

void CmdO2Done(void)
{
    CmdEnvStoreDone();
}

const char *CmdO2InitError(void) { return os2_init_error; }

/*
 * M29H direct-C/386 console surface.
 *
 * M29B-M29G recovered and proved Microsoft C/386's 32->16 migration helper
 * shape for the classic VIO/KBD entry points below.  M29H extends the same
 * descriptor-driven bridge to the cursor/mode/scroll calls used by CLS.  Declare those historical
 * interfaces explicitly so C/386 emits its real _far16 _pascal helpers;
 * OS2HOST32 recognizes the helpers from the LE fixup graph and redirects them
 * to the native VIOCALLS/KBDCALLS personality DLLs without executing 286 code.
 *
 * Keep the public CmdO2KbdKeyInfo layout host-neutral.  The historical
 * KBDKEYINFO ABI is packed on a two-byte boundary and is exactly ten bytes, so
 * use a private wire object and copy fields after the call instead of assuming
 * the public structure happens to have the same compiler padding.
 */
#pragma pack(2)
struct CmdO2KbdWire {
    UCHAR  chChar;
    UCHAR  chScan;
    UCHAR  fbStatus;
    UCHAR  bNlsShift;
    USHORT fsState;
    ULONG  time;
};

/*
 * VioGetMode writes the fixed prefix through vres before returning.  Keep a
 * private twelve-byte prefix instead of depending on the SDK's VIOMODEINFO
 * spelling/layout; CmdO2VioClear needs only row and col.
 */
struct CmdO2VioModeWire {
    USHORT cb;
    UCHAR  fbType;
    UCHAR  color;
    USHORT col;
    USHORT row;
    USHORT hres;
    USHORT vres;
};
#pragma pack()

typedef char CmdO2KbdWire_must_be_10_bytes[
    (sizeof(struct CmdO2KbdWire) == 10) ? 1 : -1];
typedef char CmdO2VioModeWire_must_be_12_bytes[
    (sizeof(struct CmdO2VioModeWire) == 12) ? 1 : -1];

USHORT _far16 _pascal VIOSCROLLUP(USHORT top,
                                  USHORT left,
                                  USHORT bottom,
                                  USHORT right,
                                  USHORT lines,
                                  UCHAR _far16 *cell,
                                  USHORT hvio);
USHORT _far16 _pascal VIOGETCURPOS(USHORT _far16 *row,
                                   USHORT _far16 *col,
                                   USHORT hvio);
USHORT _far16 _pascal VIOSETCURPOS(USHORT row,
                                   USHORT col,
                                   USHORT hvio);
USHORT _far16 _pascal VIOWRTTTY(char _far16 *text,
                                USHORT count,
                                USHORT hvio);
USHORT _far16 _pascal VIOGETMODE(struct CmdO2VioModeWire _far16 *mode,
                                 USHORT hvio);
USHORT _far16 _pascal KBDCHARIN(struct CmdO2KbdWire _far16 *info,
                                USHORT wait,
                                USHORT hkbd);
USHORT _far16 _pascal KBDFLUSHBUFFER(USHORT hkbd);

CmdO2Rc CmdO2VioWrtTTY(const char *text, unsigned long count)
{
    USHORT chunk;
    USHORT rc;

    if (text == 0 && count != 0UL)
        return CMDO2_ERROR_INVALID_PARAMETER;

    while (count != 0UL) {
        chunk = count > 0xFFFFUL ? (USHORT)0xFFFFU : (USHORT)count;
        rc = VIOWRTTTY((char *)text, chunk, (USHORT)0);
        if (rc != 0)
            return (CmdO2Rc)rc;
        text += chunk;
        count -= (unsigned long)chunk;
    }
    return CMDO2_NO_ERROR;
}

CmdO2Rc CmdO2VioGetCurPos(unsigned short *row, unsigned short *col)
{
    USHORT wire_row;
    USHORT wire_col;
    USHORT rc;

    if (row == 0 || col == 0)
        return CMDO2_ERROR_INVALID_PARAMETER;
    wire_row = 0;
    wire_col = 0;
    rc = VIOGETCURPOS(&wire_row, &wire_col, (USHORT)0);
    if (rc != 0)
        return (CmdO2Rc)rc;
    *row = (unsigned short)wire_row;
    *col = (unsigned short)wire_col;
    return CMDO2_NO_ERROR;
}

CmdO2Rc CmdO2VioSetCurPos(unsigned short row, unsigned short col)
{
    return (CmdO2Rc)VIOSETCURPOS((USHORT)row, (USHORT)col, (USHORT)0);
}

CmdO2Rc CmdO2VioGetScreenSize(unsigned short *rows, unsigned short *cols)
{
    struct CmdO2VioModeWire mode;
    USHORT rc;

    if (rows == 0 || cols == 0)
        return CMDO2_ERROR_INVALID_PARAMETER;
    cmd_memset(&mode, 0, sizeof(mode));
    mode.cb = (USHORT)sizeof(mode);
    rc = VIOGETMODE(&mode, (USHORT)0);
    if (rc != 0)
        return (CmdO2Rc)rc;
    *rows = (unsigned short)mode.row;
    *cols = (unsigned short)mode.col;
    return CMDO2_NO_ERROR;
}

CmdO2Rc CmdO2VioClear(void)
{
    struct CmdO2VioModeWire mode;
    UCHAR cell[2];
    USHORT rc;
    USHORT bottom;
    USHORT right;

    cmd_memset(&mode, 0, sizeof(mode));
    mode.cb = (USHORT)sizeof(mode);
    rc = VIOGETMODE(&mode, (USHORT)0);
    if (rc != 0)
        return (CmdO2Rc)rc;

    bottom = mode.row != 0 ? (USHORT)(mode.row - 1U) : (USHORT)0;
    right = mode.col != 0 ? (USHORT)(mode.col - 1U) : (USHORT)0;
    cell[0] = (UCHAR)' ';
    cell[1] = (UCHAR)7;
    rc = VIOSCROLLUP((USHORT)0, (USHORT)0, bottom, right,
                     (USHORT)0xFFFFU, cell, (USHORT)0);
    if (rc != 0)
        return (CmdO2Rc)rc;
    return (CmdO2Rc)VIOSETCURPOS((USHORT)0, (USHORT)0, (USHORT)0);
}

CmdO2Rc CmdO2KbdCharIn(struct CmdO2KbdKeyInfo *info, unsigned long wait)
{
    struct CmdO2KbdWire wire;
    USHORT rc;

    if (info == 0 || wait > 0xFFFFUL)
        return CMDO2_ERROR_INVALID_PARAMETER;

    cmd_memset(info, 0, sizeof(*info));
    cmd_memset(&wire, 0, sizeof(wire));
    rc = KBDCHARIN(&wire, (USHORT)wait, (USHORT)0);
    if (rc != 0)
        return (CmdO2Rc)rc;

    info->chChar = wire.chChar;
    info->chScan = wire.chScan;
    info->fbStatus = wire.fbStatus;
    info->bNlsShift = wire.bNlsShift;
    info->fsState = wire.fsState;
    info->time = (unsigned long)wire.time;
    return CMDO2_NO_ERROR;
}

CmdO2Rc CmdO2KbdFlushBuffer(void)
{
    return (CmdO2Rc)KBDFLUSHBUFFER((USHORT)0);
}

static unsigned short packed_date(const FDATE *p)
{
    unsigned short v;
    v = 0;
    cmd_memcpy(&v, p, sizeof(v));
    return v;
}

static unsigned short packed_time(const FTIME *p)
{
    unsigned short v;
    v = 0;
    cmd_memcpy(&v, p, sizeof(v));
    return v;
}

static void decode_findbuf3(const struct CmdFileFindBuf3 *fb, struct CmdO2FindData *d)
{
    unsigned n;

    cmd_memset(d, 0, sizeof(*d));
    d->date = packed_date(&fb->fdateLastWrite);
    d->time = packed_time(&fb->ftimeLastWrite);
    d->size = (unsigned long)fb->cbFile;
    d->alloc = (unsigned long)fb->cbFileAlloc;
    d->attr = (unsigned long)fb->attrFile;

    n = (unsigned)fb->cchName;
    if (n >= sizeof(d->name))
        n = (unsigned)sizeof(d->name) - 1U;
    cmd_memcpy(d->name, fb->achName, n);
    d->name[n] = '\0';
}

CmdO2Rc CmdO2ExecPgm(char *objectName, long objectNameLen,
                     unsigned long execFlag, const char *args,
                     const char *env, struct CmdO2ResultCodes *results,
                     const char *program)
{
    RESULTCODES r;
    APIRET rc;

    r.codeTerminate = 0;
    r.codeResult = 0;
    if (env == 0)
        env = CmdEnvStoreBlock();
    rc = DosExecPgm((PCHAR)objectName,
                    (LONG)objectNameLen,
                    (ULONG)execFlag,
                    (PSZ)args,
                    (PSZ)env,
                    &r,
                    (PSZ)program);
    if (results != 0) {
        results->codeTerminate = (unsigned long)r.codeTerminate;
        results->codeResult = (unsigned long)r.codeResult;
    }
    return (CmdO2Rc)rc;
}

CmdO2Rc CmdO2QueryAppType(const char *program, unsigned long *appType)
{
    ULONG type;
    APIRET rc;

    if (program == 0 || appType == 0)
        return CMDO2_ERROR_INVALID_PARAMETER;
    type = 0UL;
    rc = DosQAppType((PSZ)program, &type);
    *appType = (unsigned long)type;
    return (CmdO2Rc)rc;
}

CmdO2Rc CmdO2WaitChild(unsigned long action, unsigned long option,
                       struct CmdO2ResultCodes *results,
                       unsigned long *pid, unsigned long childPid)
{
    RESULTCODES r;
    PID waited;
    APIRET rc;

    if (results == 0 || pid == 0)
        return CMDO2_ERROR_INVALID_PARAMETER;
    r.codeTerminate = 0;
    r.codeResult = 0;
    waited = 0;
    rc = DosWaitChild((ULONG)action, (ULONG)option, &r, &waited,
                      (PID)childPid);
    results->codeTerminate = (unsigned long)r.codeTerminate;
    results->codeResult = (unsigned long)r.codeResult;
    *pid = (unsigned long)waited;
    return (CmdO2Rc)rc;
}

CmdO2Rc CmdO2QueryProgramPath(char *buffer, unsigned long cap)
{
    const char *src;
    unsigned long n;

    if (buffer == 0 || cap == 0UL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    if (_pgmptr == (char *)0 || *_pgmptr == '\0') {
        buffer[0] = '\0';
        return CMDO2_ERROR_INVALID_PARAMETER;
    }

    src = (const char *)_pgmptr;
    n = 0;
    while (src[n] != '\0') {
        if (n + 1UL >= cap) {
            buffer[0] = '\0';
            return CMDO2_ERROR_BUFFER_OVERFLOW;
        }
        buffer[n] = src[n];
        ++n;
    }
    buffer[n] = '\0';
    return CMDO2_NO_ERROR;
}

CmdO2Rc CmdO2OpenRead(const char *path, CmdO2Handle *handle)
{
    HFILE h;
    ULONG action;
    APIRET rc;

    h = 0;
    action = 0;
    rc = DosOpen((PSZ)path,
                 &h,
                 &action,
                 0UL,
                 FILE_NORMAL,
                 OPEN_ACTION_OPEN_IF_EXISTS,
                 OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE,
                 0);
    if (rc == 0 && handle != 0)
        *handle = (CmdO2Handle)h;
    return (CmdO2Rc)rc;
}

CmdO2Rc CmdO2OpenWriteReplace(const char *path, CmdO2Handle *handle)
{
    HFILE h;
    ULONG action;
    APIRET rc;

    h = 0;
    action = 0;
    rc = DosOpen((PSZ)path,
                 &h,
                 &action,
                 0UL,
                 FILE_NORMAL,
                 OPEN_ACTION_REPLACE_IF_EXISTS | OPEN_ACTION_CREATE_IF_NEW,
                 OPEN_ACCESS_WRITEONLY | OPEN_SHARE_DENYNONE,
                 0);
    if (rc == 0 && handle != 0)
        *handle = (CmdO2Handle)h;
    return (CmdO2Rc)rc;
}

CmdO2Rc CmdO2Read(CmdO2Handle h, void *b, unsigned long n, unsigned long *a)
{
    ULONG actual;
    APIRET rc;

    actual = 0;
    rc = DosRead((HFILE)h, b, (ULONG)n, &actual);
    if (a != 0)
        *a = (unsigned long)actual;
    return (CmdO2Rc)rc;
}

CmdO2Rc CmdO2Write(CmdO2Handle h, const void *b, unsigned long n,
                    unsigned long *a)
{
    ULONG actual;
    APIRET rc;

    actual = 0;
    rc = DosWrite((HFILE)h, (PVOID)b, (ULONG)n, &actual);
    if (a != 0)
        *a = (unsigned long)actual;
    return (CmdO2Rc)rc;
}

CmdO2Rc CmdO2Close(CmdO2Handle h)
{ return (CmdO2Rc)DosClose((HFILE)h); }

CmdO2Rc CmdO2CreatePipe(CmdO2Handle *readHandle, CmdO2Handle *writeHandle,
                        unsigned long size)
{
    HFILE rh;
    HFILE wh;
    APIRET rc;

    if (readHandle == 0 || writeHandle == 0)
        return CMDO2_ERROR_INVALID_PARAMETER;
    rh = 0;
    wh = 0;
    rc = DosCreatePipe(&rh, &wh, (ULONG)size);
    if (rc == 0) {
        *readHandle = (CmdO2Handle)rh;
        *writeHandle = (CmdO2Handle)wh;
    }
    return (CmdO2Rc)rc;
}

CmdO2Rc CmdO2DupHandle(CmdO2Handle oldHandle, CmdO2Handle *newHandle)
{
    HFILE nh;
    APIRET rc;

    if (newHandle == 0)
        return CMDO2_ERROR_INVALID_PARAMETER;
    nh = (HFILE)*newHandle;
    rc = DosDupHandle((HFILE)oldHandle, &nh);
    if (rc == 0)
        *newHandle = (CmdO2Handle)nh;
    return (CmdO2Rc)rc;
}

CmdO2Rc CmdO2StdSave(unsigned long stdHandle, struct CmdO2StdToken *token)
{
    HFILE h;
    APIRET rc;

    if (token == 0 || stdHandle > 2UL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    token->value = CMDO2_HANDLE_ALLOCATE;
    token->os2_value = CMDO2_HANDLE_ALLOCATE;
    h = (HFILE)CMDO2_HANDLE_ALLOCATE;
    rc = DosDupHandle((HFILE)stdHandle, &h);
    if (rc == 0)
        token->value = (unsigned long)h;
    return (CmdO2Rc)rc;
}

CmdO2Rc CmdO2StdRedirectPath(unsigned long stdHandle, const char *path,
                             int redirType)
{
    HFILE h;
    HFILE target;
    ULONG action;
    ULONG openFlags;
    ULONG openMode;
    ULONG newpos;
    APIRET rc;

    if (path == 0 || stdHandle > 2UL)
        return CMDO2_ERROR_INVALID_PARAMETER;

    h = 0;
    action = 0;
    if (redirType == CMDO2_REDIR_INPUT) {
        openFlags = OPEN_ACTION_OPEN_IF_EXISTS;
        openMode = OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE;
    } else if (redirType == CMDO2_REDIR_OUTPUT) {
        openFlags = OPEN_ACTION_REPLACE_IF_EXISTS | OPEN_ACTION_CREATE_IF_NEW;
        openMode = OPEN_ACCESS_WRITEONLY | OPEN_SHARE_DENYNONE;
    } else if (redirType == CMDO2_REDIR_APPEND) {
        openFlags = OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_CREATE_IF_NEW;
        openMode = OPEN_ACCESS_WRITEONLY | OPEN_SHARE_DENYNONE;
    } else {
        return CMDO2_ERROR_INVALID_PARAMETER;
    }

    rc = DosOpen((PSZ)path, &h, &action, 0UL, FILE_NORMAL,
                 openFlags, openMode, 0);
    if (rc != 0)
        return (CmdO2Rc)rc;

    if (redirType == CMDO2_REDIR_APPEND) {
        newpos = 0;
        rc = DosSetFilePtr(h, 0L, FILE_END, &newpos);
        if (rc != 0) {
            DosClose(h);
            return (CmdO2Rc)rc;
        }
    }

    target = (HFILE)stdHandle;
    rc = DosDupHandle(h, &target);
    DosClose(h);
    return (CmdO2Rc)rc;
}

CmdO2Rc CmdO2StdRedirectHandle(unsigned long stdHandle, CmdO2Handle source)
{
    HFILE target;
    if (stdHandle > 2UL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    target = (HFILE)stdHandle;
    return (CmdO2Rc)DosDupHandle((HFILE)source, &target);
}

CmdO2Rc CmdO2StdRestore(unsigned long stdHandle, struct CmdO2StdToken *token)
{
    HFILE saved;
    HFILE target;
    APIRET rc;

    if (token == 0 || stdHandle > 2UL ||
        token->value == CMDO2_HANDLE_ALLOCATE)
        return CMDO2_ERROR_INVALID_PARAMETER;
    saved = (HFILE)token->value;
    target = (HFILE)stdHandle;
    rc = DosDupHandle(saved, &target);
    DosClose(saved);
    token->value = CMDO2_HANDLE_ALLOCATE;
    token->os2_value = CMDO2_HANDLE_ALLOCATE;
    return (CmdO2Rc)rc;
}

CmdO2Rc CmdO2Delete(const char *p)
{ return (CmdO2Rc)DosDelete((PSZ)p); }

CmdO2Rc CmdO2Move(const char *a, const char *b)
{ return (CmdO2Rc)DosMove((PSZ)a, (PSZ)b); }

CmdO2Rc CmdO2CreateDir(const char *p)
{ return (CmdO2Rc)DosCreateDir((PSZ)p, 0); }

CmdO2Rc CmdO2DeleteDir(const char *p)
{ return (CmdO2Rc)DosDeleteDir((PSZ)p); }

CmdO2Rc CmdO2SetCurrentDir(const char *p)
{ return (CmdO2Rc)DosSetCurrentDir((PSZ)p); }

CmdO2Rc CmdO2SetDefaultDisk(unsigned long disk)
{ return (CmdO2Rc)DosSetDefaultDisk((ULONG)disk); }

CmdO2Rc CmdO2QueryCurrentDir(char *buffer, unsigned long cap)
{
    ULONG disk;
    ULONG map;
    ULONG len;
    char sub[1024];
    const char *part;
    size_t n;
    APIRET rc;

    disk = 0;
    map = 0;
    rc = DosQueryCurrentDisk(&disk, &map);
    if (rc != 0)
        return (CmdO2Rc)rc;
    (void)map;

    len = (ULONG)sizeof(sub);
    rc = DosQueryCurrentDir(0UL, (PBYTE)sub, &len);
    if (rc != 0)
        return (CmdO2Rc)rc;
    if (disk < 1UL || disk > 26UL)
        return CMDO2_ERROR_INVALID_PARAMETER;

    part = sub;
    if (*part == '\\' || *part == '/')
        ++part;
    n = cmd_strlen(part);
    if ((unsigned long)n + 4UL > cap)
        return CMDO2_ERROR_INVALID_PARAMETER;

    buffer[0] = (char)('A' + (int)disk - 1);
    buffer[1] = ':';
    buffer[2] = '\\';
    cmd_strcpy(buffer + 3, part);
    return 0;
}

CmdO2Rc CmdO2QueryPathAttr(const char *path, unsigned long *attr)
{
    struct CmdFileStatus3 st;
    APIRET rc;

    cmd_memset(&st, 0, sizeof(st));
    rc = DosQueryPathInfo((PSZ)path, FIL_STANDARD, &st, (ULONG)sizeof(st));
    if (rc == 0 && attr != 0)
        *attr = (unsigned long)st.attrFile;
    return (CmdO2Rc)rc;
}

CmdO2Rc CmdO2FindFirst(const char *p, CmdO2FindHandle *h,
                       struct CmdO2FindData *d)
{
    HDIR hdir;
    struct CmdFileFindBuf3 fb;
    ULONG count;
    APIRET rc;

    hdir = HDIR_CREATE;
    count = 1;
    cmd_memset(&fb, 0, sizeof(fb));
    /*
     * The prerelease C/386 headers type this result-buffer parameter with
     * their own SDK find-buffer pointer.  Our wire structure is deliberately
     * private so the backend is not tied to a particular header-generation
     * typedef name.  Cross the ABI boundary as PVOID: the buffer contents are
     * still the 32-bit FIL_STANDARD / FILEFINDBUF3 layout decoded below.
     */
    rc = DosFindFirst((PSZ)p,
                      &hdir,
                      O2_FIND_ATTRS,
                      (PVOID)&fb,
                      (ULONG)sizeof(fb),
                      &count,
                      FIL_STANDARD);
    if (rc == 0 && count != 0) {
        if (h != 0)
            *h = (CmdO2FindHandle)hdir;
        if (d != 0)
            decode_findbuf3(&fb, d);
    } else if (rc == 0) {
        DosFindClose(hdir);
        rc = (APIRET)CMDO2_ERROR_NO_MORE_FILES;
    }
    return (CmdO2Rc)rc;
}

CmdO2Rc CmdO2FindNext(CmdO2FindHandle h, struct CmdO2FindData *d)
{
    struct CmdFileFindBuf3 fb;
    ULONG count;
    APIRET rc;

    count = 1;
    cmd_memset(&fb, 0, sizeof(fb));
    /* See CmdO2FindFirst: keep the private wire type out of the SDK pointer
     * type system while preserving the exact FIL_STANDARD buffer layout. */
    rc = DosFindNext((HDIR)h, (PVOID)&fb, (ULONG)sizeof(fb), &count);
    if (rc == 0 && count != 0) {
        if (d != 0)
            decode_findbuf3(&fb, d);
    } else if (rc == 0) {
        rc = (APIRET)CMDO2_ERROR_NO_MORE_FILES;
    }
    return (CmdO2Rc)rc;
}

CmdO2Rc CmdO2FindClose(CmdO2FindHandle h)
{ return (CmdO2Rc)DosFindClose((HDIR)h); }

int CmdO2PathExists(const char *p)
{
    unsigned long a;
    return CmdO2QueryPathAttr(p, &a) == 0;
}

int CmdO2IsDirectory(const char *p)
{
    unsigned long a;
    return CmdO2QueryPathAttr(p, &a) == 0 &&
           (a & CMDO2_ATTR_DIRECTORY) != 0;
}
