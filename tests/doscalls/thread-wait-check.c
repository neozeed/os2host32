/* Deterministic test of the production Win32 wait helper, without Windows. */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
typedef unsigned long DWORD;
typedef long LONG;
typedef unsigned long O2ULONG;
typedef unsigned long O2APIRET;
typedef struct FakeHandle { int done, closed; } *HANDLE;
typedef int CRITICAL_SECTION;
#define FALSE 0
#define DUPLICATE_SAME_ACCESS 2
#define INFINITE 0xffffffffUL
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define WAIT_FAILED 0xffffffffUL
static struct FakeHandle duplicate[128];
static unsigned int duplicates;
static DWORD last_timeout;
static LONG InterlockedCompareExchange(LONG *p,LONG v,LONG old)
{ LONG was=*p;if(was==old)*p=v;return was; }
static LONG InterlockedExchange(LONG *p,LONG v){LONG old=*p;*p=v;return old;}
static void InitializeCriticalSection(CRITICAL_SECTION *p){*p=0;}
static void EnterCriticalSection(CRITICAL_SECTION *p){assert(*p==0);++*p;}
static void LeaveCriticalSection(CRITICAL_SECTION *p){assert(*p==1);--*p;}
static void Sleep(DWORD ms){(void)ms;}
static DWORD GetCurrentThreadId(void){return 1;}
static HANDLE GetCurrentProcess(void){return NULL;}
static DWORD GetLastError(void){return 6;}
static int DuplicateHandle(HANDLE proc,HANDLE h,HANDLE target,HANDLE *out,DWORD access,int inherit,DWORD flags)
{(void)proc;(void)target;(void)access;(void)inherit;(void)flags;assert(!h->closed);duplicate[duplicates]=*h;*out=&duplicate[duplicates++];return 1;}
static int CloseHandle(HANDLE h){assert(!h->closed);h->closed=1;return 1;}
static DWORD WaitForMultipleObjects(DWORD n,HANDLE *h,int all,DWORD timeout)
{DWORD i;(void)all;last_timeout=timeout;for(i=0;i<n;++i)if(h[i]->done)return i;return WAIT_TIMEOUT;}
#include "../../common/win32/os2_thread_wait.h"
int main(void)
{
 struct FakeHandle running={0,0},finished={1,0};O2ULONG tid;unsigned int i;
 o2_threads[0].handle=&running;o2_threads[0].tid=2;
 o2_threads[1].handle=&finished;o2_threads[1].tid=3;
 tid=2;assert(o2win_DosWaitThread(&tid,1)==294&&tid==2&&!running.closed&&last_timeout==0);
 tid=0;assert(!o2win_DosWaitThread(&tid,1)&&tid==3&&finished.closed&&!running.closed);
 tid=3;assert(o2win_DosWaitThread(&tid,0)==309);
 running.done=1;tid=2;assert(!o2win_DosWaitThread(&tid,0)&&running.closed&&last_timeout==INFINITE);
 tid=0;assert(o2win_DosWaitThread(&tid,1)==309);
 tid=1;assert(o2win_DosWaitThread(&tid,0)==309);
 assert(o2win_DosWaitThread(NULL,0)==87&&o2win_DosWaitThread(&tid,2)==87);
 for(i=0;i<duplicates;++i)assert(duplicate[i].closed);
 puts("PASS thread wait: completed-before-wait, NOWAIT, any-thread, invalid/self, handle cleanup");
 return 0;
}
