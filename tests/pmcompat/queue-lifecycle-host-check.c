/* Execute the production PM queue APIs with controlled thread/USER32 state. */
#include <stdio.h>
#include <string.h>
#define __cdecl
typedef unsigned long DWORD;
typedef unsigned long O2ULONG;
typedef unsigned long O2HAB;
typedef unsigned long O2HMQ;
typedef long O2LONG;
typedef struct { int dummy; } MSG;
#define WM_USER 1024
#define PM_NOREMOVE 0
struct PMCompatThreadState { DWORD error,queue_accel; int queue_active; };
static struct PMCompatThreadState states[2];
static unsigned thread_index,peek_calls;
static int no_memory;
static struct PMCompatThreadState *pm_thread_state(void)
{ return no_memory?NULL:&states[thread_index]; }
static DWORD GetCurrentThreadId(void) { return 100+thread_index; }
static int PeekMessageA(MSG *m,void *window,unsigned first,unsigned last,unsigned flags)
{ (void)m;(void)window;(void)first;(void)last;(void)flags;++peek_calls;return 0; }
static O2ULONG pm_api_error(DWORD error)
{ struct PMCompatThreadState *s=pm_thread_state();if(s) s->error=error;return 0; }
static void pm_trace(const char *name,unsigned long a,unsigned long b,unsigned long c)
{ (void)name;(void)a;(void)b;(void)c; }
#include "../../dlls/pmwin/pm_queue.h"
#define CHECK(c) do { ++checks; if(!(c)) { printf("FAIL line %d: %s\n",__LINE__,#c);return 1; } } while(0)
int main(void)
{
    int checks=0;O2HMQ first,other;
    CHECK(!WinDestroyMsgQueue(100) && states[0].error==0x1002);
    CHECK(!WinCreateMsgQueue(1,-1) && states[0].error==0x1018);
    CHECK(!peek_calls && !states[0].queue_active);
    first=WinCreateMsgQueue(1,0);CHECK(first==100 && states[0].queue_active && peek_calls==1);
    states[0].queue_accel=1234;
    CHECK(!WinCreateMsgQueue(1,0) && states[0].error==0x1052);
    CHECK(!WinCreateMsgQueue(1,100) && states[0].error==0x1052);
    CHECK(peek_calls==1 && states[0].queue_accel==1234 && states[0].queue_active);
    thread_index=1;
    CHECK(!WinDestroyMsgQueue(first) && states[1].error==0x1002);
    other=WinCreateMsgQueue(1,0);CHECK(other==101 && peek_calls==2);
    CHECK(!WinDestroyMsgQueue(first) && states[1].queue_active);
    CHECK(WinDestroyMsgQueue(other) && !states[1].queue_active);
    CHECK(!WinDestroyMsgQueue(other));
    thread_index=0;
    CHECK(states[0].queue_active && states[0].queue_accel==1234);
    CHECK(WinDestroyMsgQueue(first) && !states[0].queue_active && !states[0].queue_accel);
    CHECK(!WinDestroyMsgQueue(first));
    CHECK(WinCreateMsgQueue(1,0)==first && peek_calls==3);
    states[0].queue_accel=99;
    CHECK(WinTerminate(1) && !states[0].queue_active && !states[0].queue_accel);
    CHECK(WinCreateMsgQueue(1,0)==first && peek_calls==4);
    no_memory=1;CHECK(!WinCreateMsgQueue(1,0));no_memory=0;
    CHECK(WinDestroyMsgQueue(first));
    printf("PM queue lifecycle: %d checks PASS (production API, controlled per-thread state)\n",checks);
    return 0;
}
