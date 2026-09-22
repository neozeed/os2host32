/*
 * cmdos2_session_os2.c - direct Microsoft C/386 session-manager boundary.
 *
 * M29L1 exposes the complete 60-byte 32-bit STARTDATA wire while preserving
 * the simple M29L wrapper.  Keep the SESMGR dependency in this separate object
 * so older cmdos2_os2 regressions remain independent of the session manager.
 */

#define INCL_DOSPROCESS
#include <os2.h>

#include "cmdos2.h"
#include "cmdos2_env.h"

#pragma pack(2)
struct CmdO2StartDataWire {
    USHORT Length;
    USHORT Related;
    USHORT FgBg;
    USHORT TraceOpt;
    PSZ PgmTitle;
    PSZ PgmName;
    PBYTE PgmInputs;
    PBYTE TermQ;
    PBYTE Environment;
    USHORT InheritOpt;
    USHORT SessionType;
    PSZ IconFile;
    ULONG PgmHandle;
    USHORT PgmControl;
    USHORT InitXPos;
    USHORT InitYPos;
    USHORT InitXSize;
    USHORT InitYSize;
    USHORT Reserved;
    PSZ ObjectBuffer;
    ULONG ObjectBuffLen;
};
#pragma pack()

typedef char CmdO2StartDataWire_must_be_60_bytes[
    (sizeof(struct CmdO2StartDataWire) == 60) ? 1 : -1];

/* Do not depend on which prerelease header generation first exposed the
 * 32-bit DosStartSession prototype.  The recovered OS/2 2.0 CMD imports
 * SESMGR ordinal 17 and this is the 32-bit APIENTRY ABI. */
extern APIRET APIENTRY DOSSTARTSESSION(PVOID startData, PULONG sessionId,
                                         PULONG pid);
extern APIRET APIENTRY DOSSTOPSESSION(ULONG scope, ULONG sessionId);
extern APIRET APIENTRY O2HOSTQUERYSESSIONS(PVOID entries, ULONG capacity,
                                            PULONG count);

static void session_memset(void *vd, int c, unsigned n)
{
    unsigned char *d;
    d = (unsigned char *)vd;
    while (n-- != 0U)
        *d++ = (unsigned char)c;
}

static int start_options_valid(const struct CmdO2StartOptions *o)
{
    if (o == 0 || o->program == 0 || *o->program == '\0')
        return 0;
    if (o->related > 1UL || o->fgbg > 1UL || o->traceOpt > 0xFFFFUL ||
        o->inheritOpt > 1UL || o->sessionType > 0xFFFFUL ||
        o->pgmControl > 0xFFFFUL || o->initXPos > 0xFFFFUL ||
        o->initYPos > 0xFFFFUL || o->initXSize > 0xFFFFUL ||
        o->initYSize > 0xFFFFUL)
        return 0;
    if (o->objectBufferLen != 0UL && o->objectBuffer == 0)
        return 0;
    return 1;
}

CmdO2Rc CmdO2StartSessionEx(const struct CmdO2StartOptions *o,
                            unsigned long *sessionId, unsigned long *pid)
{
    struct CmdO2StartDataWire sd;
    ULONG sid;
    ULONG childPid;
    APIRET rc;
    const char *env;

    if (!start_options_valid(o) || sessionId == 0 || pid == 0)
        return CMDO2_ERROR_INVALID_PARAMETER;

    session_memset(&sd, 0, sizeof(sd));
    sd.Length = (USHORT)sizeof(sd);
    sd.Related = (USHORT)o->related;
    sd.FgBg = (USHORT)o->fgbg;
    sd.TraceOpt = (USHORT)o->traceOpt;
    sd.PgmTitle = (PSZ)o->title;
    sd.PgmName = (PSZ)o->program;
    sd.PgmInputs = (PBYTE)o->inputs;
    sd.TermQ = (PBYTE)o->termQueue;
    env = o->environment;
    if (env == 0 && o->inheritOpt == CMDO2_SSF_INHERTOPT_PARENT)
        env = CmdEnvStoreBlock();
    sd.Environment = (PBYTE)env;
    sd.InheritOpt = (USHORT)o->inheritOpt;
    sd.SessionType = (USHORT)o->sessionType;
    sd.IconFile = (PSZ)o->iconFile;
    sd.PgmHandle = (ULONG)o->pgmHandle;
    sd.PgmControl = (USHORT)o->pgmControl;
    sd.InitXPos = (USHORT)o->initXPos;
    sd.InitYPos = (USHORT)o->initYPos;
    sd.InitXSize = (USHORT)o->initXSize;
    sd.InitYSize = (USHORT)o->initYSize;
    sd.Reserved = (USHORT)0;
    sd.ObjectBuffer = (PSZ)o->objectBuffer;
    sd.ObjectBuffLen = (ULONG)o->objectBufferLen;

    sid = 0UL;
    childPid = 0UL;
    rc = DOSSTARTSESSION((PVOID)&sd, &sid, &childPid);
    *sessionId = (unsigned long)sid;
    *pid = (unsigned long)childPid;
    return (CmdO2Rc)rc;
}

CmdO2Rc CmdO2StartSession(const char *title, const char *program,
                          const char *inputs, const char *env,
                          unsigned long related, unsigned long fgbg,
                          unsigned long sessionType,
                          unsigned long *sessionId, unsigned long *pid)
{
    struct CmdO2StartOptions o;

    session_memset(&o, 0, sizeof(o));
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
    APIRET rc;
    ULONG n;

    if (count == 0 || (capacity != 0UL && entries == 0))
        return CMDO2_ERROR_INVALID_PARAMETER;
    n = 0UL;
    rc = O2HOSTQUERYSESSIONS((PVOID)entries, (ULONG)capacity, &n);
    *count = (unsigned long)n;
    return (CmdO2Rc)rc;
}

CmdO2Rc CmdO2StopSession(unsigned long taskId)
{
    if (taskId == 0UL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    return (CmdO2Rc)DOSSTOPSESSION(0UL, (ULONG)taskId);
}
