#define _POSIX_C_SOURCE 200809L
/* Test doubles implement only WHP registers and time; scheduler, allocator,
   mutexes, queues and thread lifecycle below are extracted production code. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
typedef uint16_t UINT16;
typedef uint32_t DWORD;
typedef int HRESULT;
typedef int WHV_REGISTER_NAME;
typedef struct { uint64_t Base; uint32_t Limit; uint16_t Selector,Attributes; } WHV_X64_SEGMENT_REGISTER;
typedef union { uint64_t Reg64; WHV_X64_SEGMENT_REGISTER Segment; } WHV_REGISTER_VALUE;
#define SUCCEEDED(x) ((x)==0)
#define FAILED(x) ((x)!=0)
#define E_FAIL 1
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
static WHV_REGISTER_VALUE cpu[16];
static const WHV_REGISTER_NAME thread_reg_names[16]={0};
static uint64_t clock_ms=100;
static int fail_get;
static uint64_t GetTickCount64(void) { return clock_ms; }
static void Sleep(DWORD ms) { clock_ms+=ms; }
static void fatal(const char *msg) { fprintf(stderr,"FATAL: %s\n",msg);abort(); }
static void failed(const char *what,HRESULT hr) { (void)what;(void)hr;assert(0); }
static HRESULT WHvGetVirtualProcessorRegisters(void *p,int vp,const WHV_REGISTER_NAME *n,
 unsigned count,WHV_REGISTER_VALUE *out)
{ (void)p;(void)vp;(void)n; if(fail_get) return 1; memcpy(out,cpu,count*sizeof(*out));return 0; }
static HRESULT WHvSetVirtualProcessorRegisters(void *p,int vp,const WHV_REGISTER_NAME *n,
 unsigned count,const WHV_REGISTER_VALUE *in)
{ (void)p;(void)vp;(void)n;memcpy(cpu,in,count*sizeof(*in));return 0; }
#include "pm-win32-mock.h"
#include "console-mock.h"
#include "process-mock.h"
#include "defs.inc"
struct HostStub { uint32_t ordinal,address; };
struct Runtime {
 struct GuestPM *pm;
 struct GuestGPI *gpi;
 void *partition; uint8_t *ram; int std_closed[3];
 struct HostStub doscall_stubs[MAX_IMPORTS]; uint32_t stub_next;
 struct GuestAlloc allocs[MAX_ALLOCS]; uint32_t alloc_next;
 struct GuestThread threads[MAX_THREADS]; struct GuestEventSem events[MAX_EVENT_SEMS];
 struct GuestMutex mutexes[MAX_MUTEXES]; struct GuestQueue queues[MAX_QUEUES];
 uint64_t wait_serial; uint32_t next_tid;
 int current_thread,switch_requested,current_thread_exited;
 int process_exited;uint32_t process_rc;
};
static HANDLE os2_handle(struct Runtime *rt,uint32_t h){(void)rt;return GetStdHandle(h);}
static uint8_t *load_file(const char *name,uint32_t *size){uint8_t *p;(void)name;*size=512;p=calloc(1,*size);p[0]='M';p[1]='Z';p[60]=64;p[64]='L';p[65]='E';p[72]=2;p[74]=1;return p;}
static void gpi_cleanup(struct Runtime *rt);
static int gpi_window_ps(struct Runtime *rt,uint32_t id);
#include "production.inc"
static struct Runtime rt;
static void select_thread(unsigned slot)
{
 if(rt.current_thread>=0 && rt.threads[rt.current_thread].state==THREAD_RUNNING)
   rt.threads[rt.current_thread].state=THREAD_RUNNABLE;
 rt.current_thread=(int)slot; rt.threads[slot].state=THREAD_RUNNING;
 assert(load_thread_context(&rt,&rt.threads[slot])==0);
 rt.switch_requested=rt.current_thread_exited=0;
}
static void park(uint32_t rc)
{
 cpu[3].Reg64=rc;
 assert(save_thread_context(&rt,current_guest_thread(&rt))==0);
}
static uint32_t mcall(unsigned n,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{ return dispatch_mutex(&rt,n,a,b,c,d); }
static uint32_t qcall(unsigned n,uint32_t a,uint32_t b,uint32_t c,uint32_t d,
 uint32_t e,uint32_t f,uint32_t g,uint32_t h)
{
 uint32_t v[8]={a,b,c,d,e,f,g,h};unsigned i;
 for(i=0;i<8;++i)guest_put_u32(&rt,0x804+i*4,v[i]);
 return dispatch_queue(&rt,n,0x800);
}
#define QR(q,wait) qcall(9,q,0xa00,0xa08,0xa0c,0,wait,0xa10,0)
static void reset(void)
{
 uint8_t *ram=rt.ram;
 clock_ms=100;
 memset(&rt,0,sizeof(rt)); rt.ram=ram; memset(ram,0,RAM_SIZE);
 rt.stub_next=GUEST_STUB_BASE;
 rt.alloc_next=GUEST_ALLOC_BASE; rt.next_tid=2;rt.current_thread=0;
 rt.threads[0].tid=1; rt.threads[0].state=THREAD_RUNNING;
 rt.threads[0].stack_base=0x20000;rt.threads[0].stack_size=0x8000;rt.threads[0].regs_valid=1;
 init_process_info(&rt,0xe00000,0xe00100,0x200);init_thread_info(&rt,0);
 assert(load_thread_context(&rt,&rt.threads[0])==0);
}
static void spawn(void) { assert(create_guest_thread(&rt,0x500,0x10000,0,0,8192)==0); }
#include "pm-check.inc"
#include "gpi-check.inc"
#include "sarien-check.inc"
#include "console-check.inc"
#include "process-check.inc"
int main(void)
{
 uint32_t h,old,q,q2,ev,i,j,base,end,tid,deadline,token;
 uint32_t args[2]={123,456};
 WHV_REGISTER_VALUE original[THREAD_REG_COUNT];
 DWORD ms;
 rt.ram=calloc(1,RAM_SIZE);assert(rt.ram);test_console();test_process();reset();spawn();spawn();
 h=hostcall_stub(&rt,HC_DOSCALLS,9);q=hostcall_stub(&rt,HC_QUECALLS,9);
 assert(h!=q && hostcall_stub(&rt,HC_QUECALLS,9)==q);
 assert(guest_u32(&rt,h+1)==(HC_DOSCALLS|9) && guest_u32(&rt,q+1)==(HC_QUECALLS|9));
 /* R7 regressions on all contexts, checking saved FS and private markers. */
 for(i=3;i<MAX_THREADS;i++)spawn();
 for(i=0;i<MAX_THREADS;i++) {
   guest_put_u32(&rt,info_tib_address(i),0x40000+i*16);
   rt.threads[i].regs[3].Reg64=0xabc000+i;
 }
 for(j=0;j<1000;j++)for(i=0;i<MAX_THREADS;i++) {
   select_thread(i);
   assert(cpu[3].Reg64==0xabc000+i);
   assert(cpu[14].Segment.Base==info_tib_address(i));
   assert(get_info_blocks(&rt,0x500,0x504)==0);
   assert(guest_u32(&rt,0x500)==info_tib_address(i));
   assert(guest_u32(&rt,0x504)==GUEST_INFO);
   assert(guest_u32(&rt,info_tib_address(i))==0x40000+i*16);
   assert(save_thread_context(&rt,current_guest_thread(&rt))==0);
 }
 assert(get_info_blocks(&rt,0,0)==0);
 guest_put_u32(&rt,0x500,0xdeadbeef);
 assert(get_info_blocks(&rt,0x500,0xfffffffe)==87);
 assert(guest_u32(&rt,0x500)==0xdeadbeef);
 reset();spawn();spawn();
 /* Host -> guest callback continuations, two threads and nested returns. */
 reset();spawn();
 cpu[0].Reg64=0x11000;cpu[1].Reg64=0x27000;cpu[2].Reg64=0x602;
 for(i=3;i<10;i++)cpu[i].Reg64=0xaa000000+i;
 memcpy(original,cpu,sizeof(cpu));
 assert(begin_guest_callback(&rt,0,args,2,0x600,0x11002)==87);
 assert(!rt.threads[0].callbacks);
 assert(begin_guest_callback(&rt,0x12000,args,2,0x600,0x11002)==0);
 assert(cpu[0].Reg64==0x12000 && cpu[1].Reg64==0x26ff4);
 assert(!(cpu[2].Reg64&0x400));
 assert(guest_u32(&rt,0x26ff4)==GUEST_CALLBACK_RETURN_STUB);
 assert(guest_u32(&rt,0x26ff8)==123 && guest_u32(&rt,0x26ffc)==456);
 assert(guest_sleep(&rt,20)==0);park(0);select_thread(1);
 assert(begin_guest_callback(&rt,0x13000,args,2,0x604,0x14002)==0);
 assert(rt.threads[0].callback_depth==1 && rt.threads[1].callback_depth==1);
 base=rt.threads[1].callbacks->entry_sp;guest_put_u32(&rt,base,0x87654321);
 assert(return_guest_callback(&rt,base,GUEST_CALLBACK_RETURN_STUB+6)==0);
 assert(guest_u32(&rt,0x604)==0x87654321);
 clock_ms+=20;wake_expired_waiters(&rt);select_thread(0);
 /* Simulate callback stack frames before it requests a nested callback. */
 cpu[1].Reg64-=64;
 assert(begin_guest_callback(&rt,0x15000,args,2,0x608,0x16002)==0);
 base=rt.threads[0].callbacks->entry_sp;
 assert(return_guest_callback(&rt,base,GUEST_CALLBACK_RETURN_STUB)==87);
 assert(rt.threads[0].callback_depth==2);
 guest_put_u32(&rt,base,0x12345678);
 assert(return_guest_callback(&rt,base,GUEST_CALLBACK_RETURN_STUB+6)==0);
 assert(cpu[0].Reg64==0x16002 && rt.threads[0].callback_depth==1);
 base=rt.threads[0].callbacks->entry_sp;guest_put_u32(&rt,base,0xfedcba98);
 /* Callback clobbers integer and segment registers: continuation restores all. */
 memset(cpu,0x55,sizeof(cpu));
 assert(return_guest_callback(&rt,base,GUEST_CALLBACK_RETURN_STUB+6)==0);
 original[0].Reg64=0x11002;original[3].Reg64=0;
 assert(memcmp(cpu,original,sizeof(cpu))==0);
 assert(guest_u32(&rt,0x600)==0xfedcba98 && !rt.threads[0].callbacks);
 assert(return_guest_callback(&rt,base,GUEST_CALLBACK_RETURN_STUB+6)==87);
 for(i=0;i<16;i++)assert(begin_guest_callback(&rt,0x12000,args,2,0x600,0x11002)==0);
 assert(begin_guest_callback(&rt,0x12000,args,2,0x600,0x11002)==8);
 cancel_guest_callbacks(&rt.threads[0]);assert(!rt.threads[0].callback_depth);
 cpu[1].Reg64=rt.threads[0].stack_base+12;
 assert(begin_guest_callback(&rt,0x12000,args,2,0x600,0x11002)==87);
 reset();spawn();select_thread(1);
 assert(begin_guest_callback(&rt,0x12000,args,2,0x600,0x11002)==0);
 finish_guest_thread(&rt);assert(!rt.threads[1].callbacks && rt.threads[1].callback_depth==0);
 select_thread(0);spawn();assert(!rt.threads[1].callbacks);
 reset();spawn();spawn();
 /* Sleep(0) rotates; all-blocked positive sleep advances only to deadline. */
 assert(guest_sleep(&rt,0)==0);assert(switch_after_hypercall(&rt)==0);assert(rt.current_thread==1);
 assert(guest_sleep(&rt,20)==0);park(0);select_thread(2);
 assert(guest_sleep(&rt,10)==0);park(0);select_thread(0);
 assert(guest_sleep(&rt,30)==0);park(0);
 assert(next_wait_timeout(&rt,&ms) && ms==10);
 assert(schedule_next_thread(&rt)==0 && rt.current_thread==2 && clock_ms==110);
 clock_ms=120;assert(wake_expired_waiters(&rt));assert(rt.threads[1].state==THREAD_RUNNABLE);
 clock_ms=130;assert(wake_expired_waiters(&rt));
 /* Mutex recursive ownership, queue order, nonowner, timeout and abandonment. */
 select_thread(0);assert(mcall(331,0,0x500,0,1)==0);h=guest_u32(&rt,0x500);
 assert(mcall(334,h,0,0,0)==0);assert(mcall(336,h,0x510,0x514,0x518)==0);
 assert(guest_u32(&rt,0x510)==1 && guest_u32(&rt,0x514)==1 && guest_u32(&rt,0x518)==2);
 select_thread(1);assert(mcall(335,h,0,0,0)==288);assert(mcall(334,h,0,0,0)==121);
 assert(mcall(334,h,10,0,0)==0);park(0);deadline=(uint32_t)clock_ms+10;
 select_thread(2);assert(mcall(334,h,SEM_INDEFINITE_WAIT,0,0)==0);park(0);
 select_thread(0);assert(mcall(333,h,0,0,0)==301);
 clock_ms=deadline;wake_expired_waiters(&rt);
 assert(rt.threads[1].regs[3].Reg64==121 && rt.threads[1].state==THREAD_RUNNABLE);
 assert(mcall(335,h,0,0,0)==0);assert(mutex_from_handle(&rt,h)->owner==1);
 assert(mcall(335,h,0,0,0)==0);assert(mutex_from_handle(&rt,h)->owner==3);
 select_thread(2);finish_guest_thread(&rt);select_thread(0);
 assert(mcall(334,h,0,0,0)==105);assert(mcall(335,h,0,0,0)==0);
 old=h;assert(mcall(333,h,0,0,0)==0);assert(mcall(331,0,0x500,0,0)==0);
 h=guest_u32(&rt,0x500);assert(h!=old);assert(mcall(334,old,0,0,0)==6);
 assert(mcall(333,h,0,0,0)==0);
 /* A dying owner hands the abandoned mutex directly to a blocked peer. */
 reset();spawn();spawn();select_thread(1);
 strcpy((char*)rt.ram+0x900,"\\SEM32\\CHECK");
 assert(mcall(331,0x900,0x500,0,1)==0);h=guest_u32(&rt,0x500);
 assert(mcall(332,0x900,0x504,0,0)==0);assert(mcall(333,h,0,0,0)==0);
 select_thread(2);assert(mcall(334,h,100,0,0)==0);park(0);
 select_thread(1);finish_guest_thread(&rt);
 assert(mutex_from_handle(&rt,h)->owner==3);
 assert(rt.threads[2].regs[3].Reg64==105 && rt.threads[2].state==THREAD_RUNNABLE);
 select_thread(2);assert(mcall(335,h,0,0,0)==0);assert(mcall(333,h,0,0,0)==0);
 /* Slot and allocation reuse, failed get-register rollback, fresh TIB/FS. */
 reset();spawn();base=rt.threads[1].stack_base;end=rt.alloc_next;
 for(i=0;i<5000;i++) {
   select_thread(1);tid=rt.threads[1].tid;
   assert(guest_u32(&rt,info_tib_address(1)+0x40)==tid);
   assert(cpu[14].Segment.Base==info_tib_address(1));
   finish_guest_thread(&rt);assert(!find_alloc(&rt,base)->used);
   select_thread(0);guest_put_u32(&rt,0x500,tid);assert(guest_wait_thread(&rt,0x500,0)==0);
   spawn();assert(rt.threads[1].stack_base==base && rt.alloc_next==end);
   assert(rt.threads[1].tid>tid);
   assert(guest_wait_thread(&rt,0x500,1)==294); /* spawn overwrote output with live TID */
 }
 select_thread(1);finish_guest_thread(&rt);select_thread(0);fail_get=1;
 assert(create_guest_thread(&rt,0x500,0x10000,0,0,8192)==1);fail_get=0;
 assert(!find_alloc(&rt,base)->used);spawn();
 assert(create_guest_thread(&rt,0x500,0x10000,0,0,0xffffffff)==87);
 /* Queue ordering, peek token, named open/refclose, stale generations. */
 reset();strcpy((char*)rt.ram+0x900,"\\QUEUES\\CHECK");
 assert(qcall(16,0x500,0,0x900,0,0,0,0,0)==0);q=guest_u32(&rt,0x500);
 assert(qcall(16,0x500,0,0x900,0,0,0,0,0)==332);
 assert(qcall(15,0x504,0x508,0x900,0,0,0,0,0)==0);assert(guest_u32(&rt,0x508)==q);
 assert(qcall(11,q,0,0,0,0,0,0,0)==0);assert(queue_from_handle(&rt,q));
 assert(QR(q,1)==342);
 assert(qcall(14,q,11,7,0xdeadbeef,2,0,0,0)==0);
 assert(qcall(14,q,12,8,0x12345678,1,0,0,0)==0);
 guest_put_u32(&rt,0xa14,0);
 assert(qcall(13,q,0xa00,0xa08,0xa0c,0xa14,1,0xa10,0)==0);
 token=guest_u32(&rt,0xa14);assert(guest_u32(&rt,0xa04)==11 && queue_from_handle(&rt,q)->count==2);
 assert(qcall(9,q,0xa00,0xa08,0xa0c,token,1,0xa10,0)==0);
 assert(guest_u32(&rt,0xa0c)==0xdeadbeef && guest_u32(&rt,0xa08)==7);
 assert(QR(q,1)==0 && guest_u32(&rt,0xa04)==12);
 for(i=0;i<QUEUE_DEPTH;i++) assert(qcall(14,q,i,0,i,0,0,0,0)==0);
 assert(qcall(14,q,0,0,0,0,0,0,0)==346);assert(qcall(10,q,0,0,0,0,0,0,0)==0);
 /* Three blocked readers, only one per element, no host-side blocking. */
 spawn();spawn();spawn();
 for(i=1;i<=3;i++){select_thread(i);assert(QR(q,0)==0);park(0);}
 select_thread(0);assert(qcall(14,q,21,1,0xcafe,0,0,0,0)==0);
 assert(rt.threads[1].state==THREAD_RUNNABLE && rt.threads[2].state==THREAD_WAIT_QUEUE);
 assert(qcall(11,q,0,0,0,0,0,0,0)==0);
 assert(rt.threads[2].regs[3].Reg64==337 && rt.threads[3].regs[3].Reg64==337);
 assert(qcall(16,0x500,2,0x900,0,0,0,0,0)==0);q2=guest_u32(&rt,0x500);assert(q!=q2);
 assert(QR(q,1)==337);
 assert(qcall(14,q2,1,0,0,1,0,0,0)==0);assert(qcall(14,q2,2,0,0,15,0,0,0)==0);
 assert(qcall(14,q2,3,0,0,15,0,0,0)==0);
 assert(QR(q2,1)==0 && guest_u32(&rt,0xa04)==2);
 assert(QR(q2,1)==0 && guest_u32(&rt,0xa04)==3);
 assert(QR(q2,1)==0 && guest_u32(&rt,0xa04)==1);
 assert(qcall(11,q2,0,0,0,0,0,0,0)==0);
 assert(qcall(16,0x500,1,0x900,0,0,0,0,0)==0);q2=guest_u32(&rt,0x500);
 assert(qcall(14,q2,1,0,0,0,0,0,0)==0);assert(qcall(14,q2,2,0,0,0,0,0,0)==0);
 assert(qcall(9,q2,RAM_SIZE-4,0xa08,0xa0c,0,1,0xa10,0)==87);
 assert(queue_from_handle(&rt,q2)->count==2);
 assert(QR(q2,1)==0 && guest_u32(&rt,0xa04)==2);
 assert(QR(q2,1)==0 && guest_u32(&rt,0xa04)==1);
 for(i=0;i<2000;i++) {
   assert(qcall(14,q2,i,55,0x90000+i,7,0,0,0)==0);
   assert(QR(q2,1)==0 && guest_u32(&rt,0xa04)==i && guest_u32(&rt,0xa0c)==0x90000+i);
 }
 /* NOWAIT notification posts guest event and wakes guest event waiters. */
 ev=event_make_handle(0,1);rt.events[0].used=1;rt.events[0].generation=1;rt.events[0].refs=1;
 assert(qcall(9,q2,0xa00,0xa08,0xa0c,0,1,0xa10,ev)==342);
 select_thread(1);sync_block(&rt,THREAD_WAIT_EVENT,ev,100);park(0);select_thread(0);
 assert(qcall(14,q2,7,0,0,0,0,0,0)==0);
 assert(rt.events[0].post_count==1 && rt.threads[1].state==THREAD_RUNNABLE);
 assert(qcall(11,q2,0,0,0,0,0,0,0)==0);assert(rt.events[0].refs==1);
 assert(get_info_blocks(&rt,0x500,0x504)==0);
 /* Wait-any completion is consumed, and a retired explicit TID still works. */
 reset();spawn();select_thread(1);tid=rt.threads[1].tid;finish_guest_thread(&rt);select_thread(0);
 guest_put_u32(&rt,0x500,0);assert(guest_wait_thread(&rt,0x500,0)==0);
 assert(guest_u32(&rt,0x500)==tid);guest_put_u32(&rt,0x500,0);
 assert(guest_wait_thread(&rt,0x500,1)==309);
 spawn();tid=rt.threads[1].tid;guest_put_u32(&rt,0x500,tid);
 assert(guest_wait_thread(&rt,0x500,0)==0);park(0);select_thread(1);finish_guest_thread(&rt);
 assert(rt.threads[0].state==THREAD_RUNNABLE && rt.threads[1].state==THREAD_FREE);
 select_thread(0);assert(guest_wait_thread(&rt,0x500,1)==0);
 finish_guest_thread(&rt);assert(rt.process_exited && rt.process_rc==0);
 reset();test_pm();reset();test_gpi();reset();test_sarien();
 free(rt.ram);puts("sync-check PASS: callback continuations + R8 scheduler/queue/mutex/recycling + R7 info");return 0;
}
