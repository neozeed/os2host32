/* Production switch-list code with controlled Win32 window lifetimes.
 * Native ordinal/ABI coverage also lives in telnetpm-api-smoke.exe. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../dlls/pmshapi/switchlist.c"

static HANDLE properties[256];
static int alive[256], fail_property;
void InitializeCriticalSection(CRITICAL_SECTION *p) { *p=0; }
void DeleteCriticalSection(CRITICAL_SECTION *p) { if(*p) abort(); }
void EnterCriticalSection(CRITICAL_SECTION *p) { if((*p)++) abort(); }
void LeaveCriticalSection(CRITICAL_SECTION *p) { if(--*p) abort(); }
BOOL IsWindow(HWND hwnd) { ULONG_PTR n=(ULONG_PTR)hwnd; return n<256 && alive[n]; }
HWND GetAncestor(HWND hwnd,unsigned flag) { (void)flag; return (ULONG_PTR)hwnd==2?(HWND)1:hwnd; }
DWORD GetCurrentProcessId(void) { return 42; }
DWORD GetWindowThreadProcessId(HWND hwnd,DWORD *pid) { *pid=(ULONG_PTR)hwnd==3?99:42; return 1; }
BOOL ProcessIdToSessionId(DWORD pid,DWORD *session) { (void)pid; *session=7; return TRUE; }
HANDLE GetPropA(HWND hwnd,const char *name) { (void)name; return IsWindow(hwnd)?properties[(ULONG_PTR)hwnd]:NULL; }
BOOL SetPropA(HWND hwnd,const char *name,HANDLE value)
{ (void)name; if(!IsWindow(hwnd)||fail_property) return FALSE; properties[(ULONG_PTR)hwnd]=value; return TRUE; }
int GetWindowTextA(HWND hwnd,char *text,int count)
{ (void)hwnd; (void)count; strcpy(text,"frame title"); return 11; }
BOOL EnumWindows(BOOL (CALLBACK *fn)(HWND,LPARAM),LPARAM context)
{ unsigned i; for(i=1;i<256;++i) if(alive[i] && i!=2 && !fn((HWND)(ULONG_PTR)i,context)) break; return TRUE; }
#define CHECK(c) do { ++checks; if(!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); return 1; } } while(0)
int main(void)
{
    struct Guarded { DWORD before; O2SwitchControl data; DWORD after; } out;
    O2SwitchControl data; DWORD h,old,handles[128]; unsigned i; int checks=0;
    pmsh_switch_init(); alive[1]=alive[2]=alive[3]=1;
    h=WinQuerySwitchHandle(1,0); CHECK(h!=0);
    CHECK(WinQuerySwitchHandle(2,42)==h && WinQuerySwitchHandle(0,42)==h);
    CHECK(!WinQuerySwitchHandle(3,0) && !WinQuerySwitchHandle(1,99));
    CHECK(!WinQuerySwitchHandle(255,0) && !WinQuerySwitchHandle(0,99));
    out.before=0x12345678; out.after=0xabcdef01;
    CHECK(!WinQuerySwitchEntry(h,&out.data) && out.before==0x12345678 && out.after==0xabcdef01);
    data=out.data;
    CHECK(data.hwnd==1 && data.idProcess==42 && data.idSession==7);
    CHECK(data.uchVisibility==4 && data.fbJump==2 && data.bProgType==3 && !strcmp(data.szSwtitle,"frame title"));
    memset(data.szSwtitle,'x',64); data.idProcess=data.idSession=0; data.uchVisibility=1; data.fbJump=1;
    data.hprog=123; data.hwndIcon=456; data.bProgType=789;
    CHECK(!WinChangeSwitchEntry(h,&data) && !WinQuerySwitchEntry(h,&out.data));
    CHECK(strlen(out.data.szSwtitle)==60 && out.data.idProcess==42 && out.data.idSession==7);
    CHECK(out.data.uchVisibility==1 && out.data.fbJump==1 && out.data.hprog==123 && out.data.hwndIcon==456 && out.data.bProgType==789);
    CHECK(WinQuerySwitchHandle(1,0)==h && !WinQuerySwitchEntry(h,&out.data) && out.data.szSwtitle[0]=='x');
    CHECK(WinQuerySwitchEntry(h,NULL)==0x1208 && WinQuerySwitchEntry(0,&out.data)==0x1202);
    CHECK(WinChangeSwitchEntry(h,NULL)==0x1208 && WinChangeSwitchEntry(0,&data)==0x1202);
    data.idProcess=99; CHECK(WinChangeSwitchEntry(h,&data)==0x1204); data.idProcess=42;
    data.hwnd=3; CHECK(!WinAddSwitchEntry(&data) && WinChangeSwitchEntry(h,&data)==0x1206); data.hwnd=1;
    CHECK(!WinRemoveSwitchEntry(h) && !WinQuerySwitchHandle(1,0));
    CHECK(WinRemoveSwitchEntry(h)==0x1202 && WinQuerySwitchEntry(h,&out.data)==0x1202);
    old=h; h=WinAddSwitchEntry(&data); CHECK(h && h!=old && WinQuerySwitchHandle(1,0)==h);
    /* Destroy/reuse a numeric HWND: old HSWITCH must never select the new window. */
    alive[1]=0; properties[1]=NULL; CHECK(WinQuerySwitchEntry(h,&out.data)==0x1202);
    alive[1]=1; old=h; h=WinQuerySwitchHandle(1,0); CHECK(h && h!=old);
    fail_property=1; CHECK(WinRemoveSwitchEntry(h)!=0 && !WinQuerySwitchEntry(h,&out.data)); fail_property=0;
    CHECK(!WinRemoveSwitchEntry(h));
    /* Each process-local window gets its own entry, with explicit capacity failure. */
    for(i=0;i<128;++i) { alive[i+10]=1; handles[i]=WinQuerySwitchHandle(i+10,0); CHECK(handles[i]!=0); }
    alive[200]=1; CHECK(!WinQuerySwitchHandle(200,0));
    CHECK(!WinRemoveSwitchEntry(handles[30])); old=WinQuerySwitchHandle(200,0); CHECK(old!=0 && old!=handles[30]);
    for(i=0;i<128;++i) if(i!=30) CHECK(!WinRemoveSwitchEntry(handles[i]));
    CHECK(!WinRemoveSwitchEntry(old)); pmsh_switch_term();
    printf("switchlist-host-check: %d checks PASS (mock Win32 window lifetime)\n",checks); return 0;
}
