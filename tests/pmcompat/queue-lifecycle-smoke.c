/* R10 native ordinal regression: per-thread queue ownership and quit flow. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#define CHECK(c) do { ++checks; if(!(c)) { printf("FAIL line %d: %s (Win32=%lu)\n",__LINE__,#c,(unsigned long)GetLastError()); return 1; } } while(0)
#define BIND(n,o) do { FARPROC p=GetProcAddress(module,(LPCSTR)(ULONG_PTR)(o));CHECK(p!=NULL);memcpy(&n,&p,sizeof(n)); } while(0)
static DWORD (__cdecl *initialize)(DWORD);
static DWORD (__cdecl *create_queue)(DWORD,LONG);
static DWORD (__cdecl *destroy_queue)(DWORD);
static DWORD (__cdecl *last_error)(DWORD);
static DWORD (__cdecl *terminate_pm)(DWORD);
static DWORD (__cdecl *post_msg)(DWORD,WORD,DWORD,DWORD);
static DWORD (__cdecl *get_msg)(DWORD,void *,DWORD,DWORD,DWORD);
static int checks;
struct Worker { DWORD foreign,own,result; };
static DWORD WINAPI worker(void *arg)
{
    struct Worker *w=(struct Worker *)arg;DWORD hab=initialize(0);
    w->result=0;
    if(!hab || destroy_queue(w->foreign) || last_error(hab)!=0x1002) return 1;
    w->own=create_queue(hab,0);
    if(!w->own || w->own==w->foreign) return 2;
    if(create_queue(hab,0) || last_error(hab)!=0x1052) return 3;
    if(!destroy_queue(w->own) || !terminate_pm(hab)) return 4;
    w->result=1;return 0;
}
int main(void)
{
    HMODULE module;DWORD hab,hmq,exitcode;HANDLE thread;struct Worker w;BYTE message[64];
    module=LoadLibraryA("PMWIN.dll");CHECK(module!=NULL);
    BIND(initialize,763);BIND(create_queue,716);BIND(destroy_queue,726);
    BIND(last_error,753);BIND(terminate_pm,888);BIND(post_msg,919);BIND(get_msg,915);
    hab=initialize(0);CHECK(hab!=0);
    hmq=create_queue(hab,0);CHECK(hmq!=0);
    CHECK(!create_queue(hab,0) && last_error(hab)==0x1052);
    CHECK(last_error(hab)==0);
    memset(&w,0,sizeof(w));w.foreign=hmq;
    thread=CreateThread(NULL,0,worker,&w,0,NULL);CHECK(thread!=NULL);
    CHECK(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0);
    CHECK(GetExitCodeThread(thread,&exitcode) && exitcode==0 && w.result==1);
    CloseHandle(thread);
    CHECK(!create_queue(hab,0) && last_error(hab)==0x1052);
    /* Posting quit exits GetMsg but DOES NOT destroy the PM queue. This is
     * the lifetime distinction required by TELNETPM's reinitialization path. */
    CHECK(post_msg(0,0x2a,0,0));
    CHECK(!get_msg(hab,message,0,0,0));
    CHECK(!create_queue(hab,0) && last_error(hab)==0x1052);
    CHECK(destroy_queue(hmq));CHECK(!destroy_queue(hmq) && last_error(hab)==0x1002);
    hmq=create_queue(hab,0);CHECK(hmq!=0);
    CHECK(terminate_pm(hab));
    CHECK(initialize(0)!=0 && create_queue(hab,0)!=0);
    CHECK(terminate_pm(hab));
    printf("R10 native PM queue ownership, worker isolation, quit and recreation: %d checks PASS\n",checks);
    return 0;
}
