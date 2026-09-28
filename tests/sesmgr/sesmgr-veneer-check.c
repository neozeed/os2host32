#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "os2_sesmgr.h"
#include "os2_sesmgr_backend.h"
#include "os2_sesmgr_win32.h"

#ifndef __cdecl
#define __cdecl
#endif

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
    uint32_t PgmHandle;
    unsigned short PgmControl;
    unsigned short InitXPos;
    unsigned short InitYPos;
    unsigned short InitXSize;
    unsigned short InitYSize;
    unsigned short Reserved;
    char *ObjectBuffer;
    uint32_t ObjectBuffLen;
};
#pragma pack(pop)

struct O2SessionInfoWire {
    uint32_t taskId;
    uint32_t pid;
    char program[OS2_SESMGR_TEXT_MAX];
    char title[OS2_SESMGR_TEXT_MAX];
};

static struct Os2SesmgrRegistry registry_store;
static struct Os2SesmgrIdentity current_id = { 50UL, 60UL, 70UL };
static struct Os2SesmgrStartRequest captured;
static int alive[512];
static int failures;

static void f_lock(void *o) { (void)o; }
static void f_unlock(void *o) { (void)o; }
static int f_rl(void *o, struct Os2SesmgrRegistry **r)
{ (void)o; *r = &registry_store; return 1; }
static void f_ru(void *o) { (void)o; }
static int f_current(void *o, struct Os2SesmgrIdentity *id)
{ (void)o; *id = current_id; return 1; }
static int f_from(void *o, Os2SesmgrNative p, uint32_t pid, struct Os2SesmgrIdentity *id)
{
    (void)p; (void)o;
    id->pid = pid; id->token_low = pid + 1UL; id->token_high = pid + 2UL;
    return 1;
}
static int f_alive_id(void *o, const struct Os2SesmgrIdentity *id)
{ (void)o; return id->pid < 512UL ? alive[id->pid] : 1; }
static int f_alive_owned(void *o, Os2SesmgrNative p)
{ (void)o; return (uint32_t)p < 512UL ? alive[(uint32_t)p] : 1; }
static Os2SesmgrApiRet f_launch(void *o, struct Os2SesmgrStartRequest *r,
                                struct Os2SesmgrLaunch *l)
{
    (void)o; captured = *r; alive[200] = 1;
    l->pid = 200UL; l->process = 200UL; l->initial_thread = 1200UL;
    l->group = r->related == OS2_SESMGR_RELATED_CHILD ? 2200UL : 0UL;
    return 0UL;
}
static Os2SesmgrApiRet f_resume(void *o, Os2SesmgrNative t)
{ (void)o; (void)t; return 0UL; }
static void f_ct(void *o, Os2SesmgrNative t) { (void)o; (void)t; }
static Os2SesmgrApiRet f_term(void *o, Os2SesmgrNative p, Os2SesmgrNative g)
{ (void)o; (void)g; if ((uint32_t)p < 512UL) alive[(uint32_t)p]=0; return 0UL; }
static void f_wait(void *o, Os2SesmgrNative p) { (void)o; (void)p; }
static void f_cp(void *o, Os2SesmgrNative p, Os2SesmgrNative g)
{ (void)o; (void)p; (void)g; }
static void f_title(void *o, const char *s) { (void)o; (void)s; }
static void f_trace(void *o, uint32_t sid, uint32_t pid)
{ (void)o; (void)sid; (void)pid; }
static const struct Os2SesmgrBackendOps ops = {
    f_lock, f_unlock, f_rl, f_ru, f_current, f_from, f_alive_id,
    f_alive_owned, f_launch, f_resume, f_ct, f_term, f_wait, f_cp,
    f_title, f_trace, f_trace
};

int os2_sesmgr_win32_init(struct Os2SesmgrSession *s)
{
    os2_sesmgr_session_init(s, NULL, &ops);
    return 1;
}
void os2_sesmgr_win32_destroy(struct Os2SesmgrSession *s)
{ os2_sesmgr_session_destroy(s); }

uint32_t __cdecl DosSmSetTitle(uint32_t, char *);
uint32_t __cdecl DosStopSession(uint32_t, uint32_t);
uint32_t __cdecl DosStartSession(struct O2StartData *, uint32_t *, uint32_t *);
uint32_t __cdecl O2HostQuerySessions(struct O2SessionInfoWire *, uint32_t,
                                     uint32_t *);

static void check(int ok, const char *msg)
{
    if (!ok) { fprintf(stderr, "FAIL: %s\n", msg); ++failures; }
}

int main(void)
{
    struct O2StartData sd;
    struct O2SessionInfoWire info;
    uint32_t sid, pid, count;
    char object[32];

    memset(&registry_store, 0, sizeof(registry_store));
    memset(alive, 0, sizeof(alive));
    alive[50] = 1;

    memset(&sd, 0, sizeof(sd));
    sd.Length = 24U;
    sd.Related = OS2_SESMGR_RELATED_CHILD;
    sd.FgBg = OS2_SESMGR_FGBG_FORE;
    sd.PgmName = "foo.exe";
    sd.PgmTitle = "Foo";
    sd.InheritOpt = 99U;
    sd.SessionType = 99U;
    sd.PgmControl = 0xffffU;
    check(DosStartSession(&sd, &sid, &pid) == 0UL && sid == 1UL && pid == 200UL,
          "24-byte STARTDATA accepted");
    check(captured.inherit_opt == OS2_SESMGR_INHERIT_SHELL &&
          captured.session_type == OS2_SESMGR_TYPE_DEFAULT &&
          captured.program_control == OS2_SESMGR_CONTROL_VISIBLE,
          "short STARTDATA defaults normalized by veneer");
    check(DosStopSession(0UL, sid) == 0UL,
          "veneer stop routes through common core");

    memset(&sd, 0, sizeof(sd));
    memset(object, 'X', sizeof(object));
    sd.Length = 60U;
    sd.Related = OS2_SESMGR_RELATED_INDEPENDENT;
    sd.FgBg = OS2_SESMGR_FGBG_BACK;
    sd.PgmName = "bar.exe";
    sd.PgmInputs = (unsigned char *)"-x";
    sd.Environment = (unsigned char *)"A=B\0\0";
    sd.InheritOpt = OS2_SESMGR_INHERIT_PARENT;
    sd.SessionType = OS2_SESMGR_TYPE_PM;
    sd.PgmControl = OS2_SESMGR_CONTROL_INVISIBLE |
                    OS2_SESMGR_CONTROL_SETPOS;
    sd.InitXPos = 1U; sd.InitYPos = 2U; sd.InitXSize = 3U; sd.InitYSize = 4U;
    sd.ObjectBuffer = object;
    sd.ObjectBuffLen = sizeof(object);
    alive[200] = 1;
    check(DosStartSession(&sd, &sid, &pid) == 0UL,
          "full STARTDATA routes through common/backend");
    check(captured.environment == (const char *)sd.Environment &&
          captured.inherit_opt == OS2_SESMGR_INHERIT_PARENT &&
          captured.session_type == OS2_SESMGR_TYPE_PM &&
          captured.init_x == 1U && captured.init_y == 2U &&
          captured.init_cx == 3U && captured.init_cy == 4U &&
          object[0] == '\0',
          "full STARTDATA fields normalized and ObjectBuffer cleared");

    memset(&info, 0, sizeof(info)); count = 0UL;
    check(O2HostQuerySessions(&info, 1UL, &count) == 0UL && count == 0UL,
          "independent session excluded from owned query");

    if (failures != 0) {
        fprintf(stderr, "sesmgr-veneer-check: %d failure(s)\n", failures);
        return 1;
    }
    puts("sesmgr-veneer-check: PASS");
    return 0;
}
