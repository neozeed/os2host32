#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "windows.h"
#include "win32_sesmgr_stub.h"

struct StubProc { DWORD pid; int alive; DWORD low; DWORD high; };
struct StubThread { int dummy; };
struct StubJob { struct StubProc *process; };
static struct StubProc current_proc = {50UL, 1, 60UL, 70UL};
static struct StubProc procs[32];
static unsigned proc_count;
static DWORD next_pid = 200UL;
static unsigned char shared_map[131072];
static int mutex_token;
static int map_token;
static DWORD last_error;

DWORD stub_last_creation_flags;
STARTUPINFOA stub_last_startup;
char stub_last_command[4096];
char stub_last_environment[512];
int stub_set_title_calls;
char stub_last_title[260];

void stub_sesmgr_reset(void)
{
    memset(procs, 0, sizeof(procs)); proc_count = 0U; next_pid = 200UL;
    memset(shared_map, 0, sizeof(shared_map));
    stub_last_creation_flags = 0UL;
    memset(&stub_last_startup, 0, sizeof(stub_last_startup));
    memset(stub_last_command, 0, sizeof(stub_last_command));
    memset(stub_last_environment, 0, sizeof(stub_last_environment));
    stub_set_title_calls = 0; memset(stub_last_title, 0, sizeof(stub_last_title));
    last_error = 0UL; current_proc.alive = 1;
}

DWORD GetEnvironmentVariableA(LPCSTR name, LPSTR out, DWORD cap)
{
    const char *v = NULL; size_t n;
    if (strcmp(name, "OS2HOST32_LOADER") == 0) v = "C:\\os2host32.exe";
    if (v == NULL) return 0UL;
    n = strlen(v);
    if (n + 1U > (size_t)cap) return (DWORD)(n + 1U);
    memcpy(out, v, n + 1U); return (DWORD)n;
}
DWORD GetModuleFileNameA(HINSTANCE h, LPSTR out, DWORD cap)
{ (void)h; (void)out; (void)cap; return 0UL; }
HANDLE GetProcessHeap(void) { return (HANDLE)(uintptr_t)1U; }
LPVOID HeapAlloc(HANDLE h, DWORD flags, SIZE_T n)
{ void *p; (void)h; p = (flags & HEAP_ZERO_MEMORY) ? calloc(1,n) : malloc(n); return p; }
BOOL HeapFree(HANDLE h, DWORD flags, LPVOID p)
{ (void)h; (void)flags; free(p); return TRUE; }
int lstrcmpiA(LPCSTR a, LPCSTR b)
{
    unsigned char ca, cb;
    do { ca=(unsigned char)*a++; cb=(unsigned char)*b++;
         ca=(unsigned char)tolower(ca); cb=(unsigned char)tolower(cb);
         if (ca != cb) return (int)ca-(int)cb; } while (ca != 0U);
    return 0;
}
void InitializeCriticalSection(CRITICAL_SECTION *c) { c->dummy=0; }
void DeleteCriticalSection(CRITICAL_SECTION *c) { (void)c; }
void EnterCriticalSection(CRITICAL_SECTION *c) { (void)c; }
void LeaveCriticalSection(CRITICAL_SECTION *c) { (void)c; }
HANDLE CreateMutexA(LPVOID a, BOOL b, LPCSTR c)
{ (void)a; (void)b; (void)c; return &mutex_token; }
HANDLE CreateFileMappingA(HANDLE a, LPVOID b, DWORD c, DWORD d, DWORD e, LPCSTR f)
{ (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return &map_token; }
LPVOID MapViewOfFile(HANDLE h, DWORD a, DWORD b, DWORD c, SIZE_T n)
{ (void)h;(void)a;(void)b;(void)c; if(n>sizeof(shared_map)) return NULL; return shared_map; }
BOOL UnmapViewOfFile(LPCVOID p) { (void)p; return TRUE; }
DWORD WaitForSingleObject(HANDLE h, DWORD t) { (void)h;(void)t; return WAIT_OBJECT_0; }
BOOL ReleaseMutex(HANDLE h) { (void)h; return TRUE; }
HANDLE GetCurrentProcess(void) { return &current_proc; }
DWORD GetCurrentProcessId(void) { return current_proc.pid; }
BOOL GetProcessTimes(HANDLE h, FILETIME *ct, FILETIME *et, FILETIME *kt, FILETIME *ut)
{
    struct StubProc *p=(struct StubProc *)h; (void)et;(void)kt;(void)ut;
    if (p == NULL)
        return FALSE;
    ct->dwLowDateTime = p->low;
    ct->dwHighDateTime = p->high;
    return TRUE;
}
HANDLE OpenProcess(DWORD access, BOOL inherit, DWORD pid)
{
    unsigned i; (void)access;(void)inherit;
    if (pid==current_proc.pid) return &current_proc;
    for(i=0;i<proc_count;++i) if(procs[i].pid==pid && procs[i].alive) return &procs[i];
    last_error=ERROR_INVALID_PARAMETER; return NULL;
}
BOOL GetExitCodeProcess(HANDLE h, DWORD *code)
{
    struct StubProc *p=(struct StubProc *)h; if(p==NULL) return FALSE;
    *code=p->alive?STILL_ACTIVE:0UL; return TRUE;
}
DWORD GetProcessId(HANDLE h)
{ struct StubProc *p=(struct StubProc *)h; return p ? p->pid : 0UL; }
BOOL CloseHandle(HANDLE h) { (void)h; return TRUE; }
BOOL CreateProcessA(LPCSTR app, LPSTR cmd, LPVOID pa, LPVOID ta, BOOL inh,
                    DWORD flags, LPVOID env, LPCSTR dir, STARTUPINFOA *si,
                    PROCESS_INFORMATION *pi)
{
    struct StubProc *p; struct StubThread *t; const char *e; size_t used;
    (void)app;(void)pa;(void)ta;(void)inh;(void)dir;
    if(proc_count>=32U) return FALSE;
    p=&procs[proc_count++]; p->pid=next_pid++; p->alive=1;
    p->low=p->pid+10UL; p->high=p->pid+20UL;
    t=(struct StubThread *)malloc(sizeof(*t)); if(!t) return FALSE;
    pi->hProcess=p; pi->hThread=t; pi->dwProcessId=p->pid; pi->dwThreadId=p->pid+1000UL;
    stub_last_creation_flags=flags; stub_last_startup=*si;
    strncpy(stub_last_command,cmd,sizeof(stub_last_command)-1U);
    memset(stub_last_environment,0,sizeof(stub_last_environment));
    if(env!=NULL) {
        e=(const char *)env; used=0U;
        while(used+1U<sizeof(stub_last_environment)) {
            size_t n=strlen(e)+1U;
            if(used+n+1U>sizeof(stub_last_environment)) break;
            memcpy(stub_last_environment+used,e,n); used+=n; e+=n;
            if(*e=='\0') { stub_last_environment[used]='\0'; break; }
        }
    }
    return TRUE;
}
HANDLE CreateJobObjectA(LPVOID a, LPCSTR b)
{ struct StubJob *j=(struct StubJob *)calloc(1,sizeof(*j)); (void)a;(void)b; return j; }
BOOL AssignProcessToJobObject(HANDLE jh, HANDLE ph)
{ ((struct StubJob *)jh)->process=(struct StubProc *)ph; return TRUE; }
DWORD ResumeThread(HANDLE h) { (void)h; return 1UL; }
BOOL TerminateJobObject(HANDLE h, unsigned int code)
{ struct StubJob *j=(struct StubJob *)h; (void)code; if(j&&j->process) j->process->alive=0; return TRUE; }
BOOL TerminateProcess(HANDLE h, unsigned int code)
{ struct StubProc *p=(struct StubProc *)h; (void)code; if(!p)return FALSE; p->alive=0; return TRUE; }
DWORD GetLastError(void) { return last_error; }
BOOL SetConsoleTitleA(LPCSTR s)
{ size_t n=strlen(s); if(n>=sizeof(stub_last_title))n=sizeof(stub_last_title)-1U;
  memcpy(stub_last_title,s,n); stub_last_title[n]='\0'; ++stub_set_title_calls; return TRUE; }
