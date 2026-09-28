#include <stdio.h>
#include <string.h>

#include "os2_sesmgr.h"
#include "os2_sesmgr_win32.h"
#include "win32_sesmgr_stub.h"

static int failures;
static void check(int ok, const char *msg)
{ if(!ok){fprintf(stderr,"FAIL: %s\n",msg);++failures;} }
static void init_req(struct Os2SesmgrStartRequest *r)
{
    memset(r,0,sizeof(*r)); r->source_length=60U;
    r->related=OS2_SESMGR_RELATED_CHILD; r->fgbg=OS2_SESMGR_FGBG_BACK;
    r->program="child.exe"; r->title="Child";
    r->inherit_opt=OS2_SESMGR_INHERIT_PARENT;
    r->session_type=OS2_SESMGR_TYPE_WINDOWABLEVIO;
    r->program_control=OS2_SESMGR_CONTROL_SETPOS|OS2_SESMGR_CONTROL_MINIMIZE;
    r->init_x=10U;r->init_y=20U;r->init_cx=80U;r->init_cy=25U;
}
int main(void)
{
    struct Os2SesmgrSession s; struct Os2SesmgrStartRequest r;
    struct Os2SesmgrSessionInfo info[2]; uint32_t sid,pid,count; char env[]="z=2\0A=1\0\0";
    stub_sesmgr_reset();
    check(os2_sesmgr_win32_init(&s),"win32 backend init");
    init_req(&r); r.environment=env;
    check(os2_sesmgr_DosStartSession(&s,&r,&sid,&pid)==0UL && sid==1UL && pid==200UL,
          "related Win32 start");
    check((stub_last_creation_flags & CREATE_NEW_PROCESS_GROUP)!=0UL &&
          (stub_last_creation_flags & CREATE_SUSPENDED)!=0UL &&
          (stub_last_creation_flags & CREATE_NEW_CONSOLE)!=0UL,
          "related VIO creation flags");
    check((stub_last_startup.dwFlags & STARTF_USESHOWWINDOW)!=0UL &&
          stub_last_startup.wShowWindow==SW_SHOWMINIMIZED &&
          (stub_last_startup.dwFlags & STARTF_USEPOSITION)!=0UL &&
          (stub_last_startup.dwFlags & STARTF_USESIZE)!=0UL,
          "STARTDATA window mapping");
    check(strcmp(stub_last_environment,"A=1")==0 &&
          strcmp(stub_last_environment+4,"z=2")==0,
          "Win32 environment block normalized/sorted");
    count=0UL; memset(info,0,sizeof(info));
    check(os2_sesmgr_QuerySessions(&s,info,2UL,&count)==0UL && count==1UL &&
          info[0].task_id==1UL && info[0].pid==200UL,
          "shared registry through Win32 backend");
    check(os2_sesmgr_DosStopSession(&s,0UL,sid)==0UL,
          "related Win32 stop");

    init_req(&r); r.related=OS2_SESMGR_RELATED_INDEPENDENT;
    r.fgbg=OS2_SESMGR_FGBG_FORE; r.session_type=OS2_SESMGR_TYPE_PM;
    r.program_control=OS2_SESMGR_CONTROL_VISIBLE; r.environment=NULL;
    check(os2_sesmgr_DosStartSession(&s,&r,&sid,&pid)==0UL && sid==2UL && pid==201UL,
          "independent PM start");
    check((stub_last_creation_flags & DETACHED_PROCESS)!=0UL &&
          (stub_last_creation_flags & CREATE_NEW_CONSOLE)==0UL &&
          (stub_last_creation_flags & CREATE_SUSPENDED)==0UL,
          "PM detached creation flags");

    os2_sesmgr_win32_destroy(&s);
    if(failures){fprintf(stderr,"sesmgr-win32-shim-check: %d failure(s)\n",failures);return 1;}
    puts("sesmgr-win32-shim-check: PASS"); return 0;
}
