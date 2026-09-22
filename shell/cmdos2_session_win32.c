/*
 * cmdos2_session_win32.c - native bootstrap boundary for SESMGR.17.
 *
 * M29L1 exposes the full STARTDATA payload through CmdO2StartSessionEx while
 * retaining the original M29L convenience wrapper.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

#include "cmdos2.h"
#include "cmdos2_env.h"

#pragma pack(push, 2)
struct CmdO2StartDataWire {
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

typedef unsigned long (__cdecl *PFN_DOSSTARTSESSION)(
    struct CmdO2StartDataWire *, unsigned long *, unsigned long *);
typedef unsigned long (__cdecl *PFN_DOSSTOPSESSION)(unsigned long,
                                                     unsigned long);
typedef unsigned long (__cdecl *PFN_O2HOSTQUERYSESSIONS)(
    struct CmdO2SessionInfo *, unsigned long, unsigned long *);

static HMODULE g_sesmgr;
static PFN_DOSSTARTSESSION g_start;
static PFN_DOSSTOPSESSION g_stop;
static PFN_O2HOSTQUERYSESSIONS g_query;

static int ensure_sesmgr(void)
{
    FARPROC proc;
    if (g_start != NULL && g_stop != NULL && g_query != NULL)
        return 1;
    if (g_sesmgr == NULL)
        g_sesmgr = LoadLibraryA("SESMGR.dll");
    if (g_sesmgr == NULL)
        return 0;
    proc = GetProcAddress(g_sesmgr, (LPCSTR)(ULONG_PTR)17);
    if (proc == NULL)
        return 0;
    g_start = (PFN_DOSSTARTSESSION)proc;
    proc = GetProcAddress(g_sesmgr, (LPCSTR)(ULONG_PTR)8);
    if (proc == NULL)
        return 0;
    g_stop = (PFN_DOSSTOPSESSION)proc;
    proc = GetProcAddress(g_sesmgr, (LPCSTR)(ULONG_PTR)1000);
    if (proc == NULL)
        return 0;
    g_query = (PFN_O2HOSTQUERYSESSIONS)proc;
    return 1;
}

static int start_options_valid(const struct CmdO2StartOptions *o)
{
    if (o == NULL || o->program == NULL || *o->program == '\0')
        return 0;
    if (o->related > 1UL || o->fgbg > 1UL || o->traceOpt > 0xFFFFUL ||
        o->inheritOpt > 1UL || o->sessionType > 0xFFFFUL ||
        o->pgmControl > 0xFFFFUL || o->initXPos > 0xFFFFUL ||
        o->initYPos > 0xFFFFUL || o->initXSize > 0xFFFFUL ||
        o->initYSize > 0xFFFFUL)
        return 0;
    if (o->objectBufferLen != 0UL && o->objectBuffer == NULL)
        return 0;
    return 1;
}

CmdO2Rc CmdO2StartSessionEx(const struct CmdO2StartOptions *o,
                            unsigned long *sessionId, unsigned long *pid)
{
    struct CmdO2StartDataWire sd;
    const char *env;

    if (!start_options_valid(o) || sessionId == NULL || pid == NULL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    if (!ensure_sesmgr())
        return 2UL;

    memset(&sd, 0, sizeof(sd));
    sd.Length = (unsigned short)sizeof(sd);
    sd.Related = (unsigned short)o->related;
    sd.FgBg = (unsigned short)o->fgbg;
    sd.TraceOpt = (unsigned short)o->traceOpt;
    sd.PgmTitle = (char *)o->title;
    sd.PgmName = (char *)o->program;
    sd.PgmInputs = (unsigned char *)o->inputs;
    sd.TermQ = (unsigned char *)o->termQueue;
    env = o->environment;
    if (env == NULL && o->inheritOpt == CMDO2_SSF_INHERTOPT_PARENT)
        env = CmdEnvStoreBlock();
    sd.Environment = (unsigned char *)env;
    sd.InheritOpt = (unsigned short)o->inheritOpt;
    sd.SessionType = (unsigned short)o->sessionType;
    sd.IconFile = (char *)o->iconFile;
    sd.PgmHandle = o->pgmHandle;
    sd.PgmControl = (unsigned short)o->pgmControl;
    sd.InitXPos = (unsigned short)o->initXPos;
    sd.InitYPos = (unsigned short)o->initYPos;
    sd.InitXSize = (unsigned short)o->initXSize;
    sd.InitYSize = (unsigned short)o->initYSize;
    sd.ObjectBuffer = o->objectBuffer;
    sd.ObjectBuffLen = o->objectBufferLen;

    *sessionId = 0UL;
    *pid = 0UL;
    return (CmdO2Rc)g_start(&sd, sessionId, pid);
}

CmdO2Rc CmdO2StartSession(const char *title, const char *program,
                          const char *inputs, const char *env,
                          unsigned long related, unsigned long fgbg,
                          unsigned long sessionType,
                          unsigned long *sessionId, unsigned long *pid)
{
    struct CmdO2StartOptions o;

    memset(&o, 0, sizeof(o));
    o.title = title;
    o.program = program;
    o.inputs = inputs;
    o.environment = env;
    o.related = related;
    o.fgbg = fgbg;
    o.traceOpt = CMDO2_SSF_TRACEOPT_NONE;
    o.inheritOpt = CMDO2_SSF_INHERTOPT_PARENT;
    o.sessionType = sessionType;
    o.pgmControl = CMDO2_SSF_CONTROL_VISIBLE;
    return CmdO2StartSessionEx(&o, sessionId, pid);
}

CmdO2Rc CmdO2QuerySessions(struct CmdO2SessionInfo *entries,
                           unsigned long capacity, unsigned long *count)
{
    if (count == NULL || (capacity != 0UL && entries == NULL))
        return CMDO2_ERROR_INVALID_PARAMETER;
    if (!ensure_sesmgr())
        return 2UL;
    return (CmdO2Rc)g_query(entries, capacity, count);
}

CmdO2Rc CmdO2StopSession(unsigned long taskId)
{
    if (taskId == 0UL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    if (!ensure_sesmgr())
        return 2UL;
    return (CmdO2Rc)g_stop(0UL, taskId);
}
