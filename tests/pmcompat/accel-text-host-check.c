#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef DWORD O2ULONG;
typedef DWORD O2HAB;
typedef DWORD O2HWND;
#define O2_RT_ACCELTABLE 8
struct PMCompatThreadState { DWORD error,queue_accel; };
static struct PMCompatThreadState states[2];
static unsigned current_thread;
static struct PMCompatThreadState *pm_thread_state(void) { return &states[current_thread]; }
static DWORD pm_api_error(DWORD e) { states[current_thread].error=e; return 0; }
static WORD pm_rd16(const unsigned char *p) { return (WORD)(p[0]|p[1]<<8); }
struct PMCompatResource { const unsigned char *data; DWORD size; };
static unsigned char resource_data[]={1,0,0xb5,1,2,2,0x20,0,0,0};
static struct PMCompatResource resource={resource_data,sizeof(resource_data)};
static const struct PMCompatResource *pm_find_resource(DWORD module,WORD type,WORD id)
{ return !module && type==8 && id==250?&resource:NULL; }
#include "../../dlls/pmwin/pm_accel.h"
#include "../../dlls/pmwin/pm_text.h"
static HANDLE properties[4];
static HWND native_hwnd(DWORD hwnd) { return (HWND)(ULONG_PTR)hwnd; }
BOOL IsWindow(HWND hwnd) { return (ULONG_PTR)hwnd>=1 && (ULONG_PTR)hwnd<4; }
static int pm_is_own_process_window(HWND hwnd) { return IsWindow(hwnd); }
HANDLE GetPropA(HWND hwnd,const char *key) { (void)key; return IsWindow(hwnd)?properties[(ULONG_PTR)hwnd]:NULL; }
BOOL SetPropA(HWND hwnd,const char *key,HANDLE value)
{ (void)key; if(!IsWindow(hwnd)) return FALSE; properties[(ULONG_PTR)hwnd]=value; return TRUE; }
HANDLE RemovePropA(HWND hwnd,const char *key)
{ HANDLE old=GetPropA(hwnd,key); if(IsWindow(hwnd)) properties[(ULONG_PTR)hwnd]=NULL; return old; }
HWND GetParent(HWND hwnd) { return hwnd==(HWND)2?(HWND)1:NULL; }
void *GetProcessHeap(void) { return (void *)1; }
void *HeapAlloc(void *h,DWORD flags,size_t size) { (void)h;(void)flags;return malloc(size); }
BOOL HeapFree(void *h,DWORD flags,void *data) { (void)h;(void)flags;free(data);return TRUE; }
void InitializeCriticalSection(CRITICAL_SECTION *p) { *p=0; }
void DeleteCriticalSection(CRITICAL_SECTION *p) { if(*p) abort(); }
void EnterCriticalSection(CRITICAL_SECTION *p) { if((*p)++) abort(); }
void LeaveCriticalSection(CRITICAL_SECTION *p) { if(--*p) abort(); }
#define CHECK(c) do { ++checks; if(!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); return 1; } } while(0)
int main(void)
{
    DWORD h,other,handles[128]; unsigned i; int checks=0;
    unsigned char copy[24],table[]={3,0,0xb5,1,0x11,0,'C',0,42,0,2,1,0x20,0,43,0,4,0,0x1e,0,44,0};
    const unsigned char text[]={0xb5,1,6,'h','e','l','l','o',0,1,0,4,'b','y','e',0};
    struct PMAccelCommand command;
    pm_accel_init(); CHECK(!WinQueryAccelTable(1,0));
    h=WinLoadAccelTable(1,0,250); CHECK(h && WinCopyAccelTable(h,NULL,0)==10);
    memset(copy,0xaa,sizeof(copy)); CHECK(WinCopyAccelTable(h,copy,10)==10 && !memcmp(copy,resource_data,10) && copy[10]==0xaa);
    CHECK(pm_accel_match_table(h,2,0,0x20,0,&command) && command.command==0 && command.message==0x22);
    CHECK(WinSetAccelTable(1,h,0) && WinQueryAccelTable(1,0)==h);
    current_thread=1; CHECK(!WinQueryAccelTable(1,0)); current_thread=0;
    CHECK(WinSetAccelTable(1,0,0) && !WinQueryAccelTable(1,0));
    other=WinCreateAccelTable(1,table); CHECK(other && other!=h);
    table[8]=99; CHECK(pm_accel_match_table(other,0x11,3,0,0,&command) && command.command==42);
    CHECK(!pm_accel_match_table(other,1,'c',0,0,&command));
    CHECK(!pm_accel_match_table(other,0x51,3,0,0,&command));
    CHECK(pm_accel_match_table(other,2,0,0x20,0,&command) && command.message==0x21 && command.command==43);
    CHECK(pm_accel_match_table(other,4,0,0,0x1e,&command) && command.command==44);
    CHECK(WinSetAccelTable(1,other,1) && WinQueryAccelTable(1,1)==other && !WinQueryAccelTable(1,3));
    CHECK(pm_accel_match_window((HWND)2,0x11,3,0,0,&command) && command.target==(HWND)1 && command.command==42);
    CHECK(!WinSetAccelTable(1,123,1) && states[0].error==0x101a && WinQueryAccelTable(1,1)==other);
    CHECK(!WinSetAccelTable(1,h,999) && states[0].error==0x1001);
    CHECK(WinDestroyAccelTable(other) && !WinQueryAccelTable(1,1));
    CHECK(!WinDestroyAccelTable(other) && !WinCopyAccelTable(other,NULL,0));
    CHECK(WinSetAccelTable(1,h,3)); properties[3]=NULL; CHECK(!WinQueryAccelTable(1,3));
    CHECK(WinDestroyAccelTable(h));
    resource.size=9; CHECK(!WinLoadAccelTable(1,0,250)); resource.size=10;
    CHECK(!WinLoadAccelTable(1,0,65536) && !WinLoadAccelTable(1,0,999));
    CHECK(!WinCreateAccelTable(1,NULL));
    for(i=0;i<128;++i) { handles[i]=WinLoadAccelTable(1,0,250); CHECK(handles[i]!=0); }
    CHECK(!WinLoadAccelTable(1,0,250));
    for(i=0;i<128;++i) CHECK(WinDestroyAccelTable(handles[i]));
    memset(copy,0xaa,sizeof(copy)); CHECK(pm_text_copy(text,sizeof(text),0,(char *)copy,6)==5 && !strcmp((char *)copy,"hello") && copy[6]==0xaa);
    CHECK(pm_text_copy(text,sizeof(text),0,(char *)copy,3)==2 && !strcmp((char *)copy,"he"));
    CHECK(!pm_text_copy(text,sizeof(text),1,(char *)copy,24) && !copy[0]);
    CHECK(pm_text_copy(text,sizeof(text),2,(char *)copy,24)==3 && !strcmp((char *)copy,"bye"));
    CHECK(!pm_text_copy(text,sizeof(text)-1,2,(char *)copy,24) && !copy[0]);
    CHECK(!pm_text_copy(text,sizeof(text),16,(char *)copy,24));
    copy[0]=0xaa; CHECK(!pm_text_copy(text,sizeof(text),0,(char *)copy,0) && copy[0]==0xaa);
    CHECK(!pm_text_copy(text,sizeof(text),0,(char *)copy,1) && !copy[0]);
    pm_accel_term(); printf("accel-text-host-check: %d checks PASS (production core, mock Win32 windows)\n",checks); return 0;
}
