#define SOFT386_TEST_PROVIDER
#define main soft386_vessel_main
#include "../src/soft386_os2.c"
#undef main
#define QUEUE_PROVIDER_ONLY
#include "queue-bridge-check.c"
#ifndef __cdecl
#define __cdecl
#endif
static struct Soft386PmBridge *pb;
static uint32_t (__cdecl *subproc)(uint32_t,uint32_t,uint32_t,uint32_t);
static unsigned subclassed,queried,drawn,scrolled,dialogs,shorts,dismissed,oldcalls;
#define NW 0xe1112222u
#define ND 0xe3334444u
#define ND2 0xe3335555u
#define NP 0xe5556666u
#define NB 0xe7778888u
#define PS 0xe999aaaau
static void hp(uintptr_t p,size_t n){struct Runtime *r=pb->opaque;assert(p&&p+n>p);assert(p+n<=(uintptr_t)r->ram||p>=(uintptr_t)r->ram+RAM_SIZE);}
static uint32_t f_init(uintptr_t n){assert(!n);return 1;}
static uint32_t f_queue(uintptr_t h,uintptr_t n){assert(h==1&&!n);return 100;}
static uint32_t f_create(uintptr_t par,uintptr_t cls,uintptr_t text,uintptr_t style,uintptr_t x,uintptr_t y,uintptr_t w,uintptr_t h,uintptr_t own,uintptr_t after,uintptr_t id,uintptr_t ctl,uintptr_t pres){assert(par==1&&cls==0xffff0005u);hp(text,4);assert(!strcmp((char *)text,"jar"));assert(!style&&!x&&!y&&w==10&&h==10&&!own&&!after&&!id&&!ctl&&!pres);return NW;}
static uint32_t oldproc(uintptr_t hwnd,uintptr_t msg,uintptr_t mp1,uintptr_t mp2){assert(hwnd==NW);assert(msg==0x100||msg==0x101||msg==2||msg==0x23);assert(mp1==(msg==0x100?NP:0)&&!mp2);oldcalls++;return (msg==0x100||msg==0x101)?NP:0;}
static uintptr_t f_subclass(uintptr_t w,uintptr_t callback){assert(w==NW&&callback);subproc=(void *)callback;subclassed++;return (uintptr_t)oldproc;}
static uint32_t f_pointer(uintptr_t ptr,uintptr_t info){uint32_t *p=(void *)info;assert(ptr==NP);hp(info,28);memset(p,0,28);p[0]=1;p[1]=4;p[2]=5;p[3]=NB;queried++;return 1;}
static uint32_t f_process(uintptr_t w,uintptr_t pid,uintptr_t tid){assert(w==NW);hp(pid,4);hp(tid,4);*(uint32_t *)pid=0xfeed1234;*(uint32_t *)tid=0xfeed5678;return 1;}
static uint32_t f_ps(uintptr_t w){assert(w==NW);return PS;}
static uint32_t f_draw(uintptr_t ps,uintptr_t bm,uintptr_t src,uintptr_t dst,uintptr_t fore,uintptr_t back,uintptr_t flags){assert(ps==PS&&bm==NB&&!src&&!fore&&!back&&!flags);hp(dst,8);drawn++;return 1;}
static uint32_t f_scroll(uintptr_t w,uintptr_t dx,uintptr_t dy,uintptr_t sr,uintptr_t cr,uintptr_t region,uintptr_t out,uintptr_t flags){assert(w==NW&&!dx&&dy==12&&!sr&&!cr&&!region&&flags==2);hp(out,16);memset((void *)out,0,16);wr32((uint8_t *)out+12,12);scrolled++;assert(subproc);subproc(NW,0x23,0,0);assert(subproc(NW,0x100,NP,0)==NP);assert(subproc(NW,0x101,0,0)==NP);return 2;}
static uint32_t f_dlg(uintptr_t par,uintptr_t own,uintptr_t callback,uintptr_t mod,uintptr_t id,uintptr_t param){uint32_t (__cdecl *fn)(uint32_t,uint32_t,uint32_t,uint32_t)=(void *)callback;struct Runtime *r=pb->opaque;assert(par==1&&!mod&&!param);dialogs++;if(id==2){assert(own==ND);fn(ND2,0x3b,0,0xbadcafe);assert(dismissed==1);return 43;}assert(own==NW&&id==1);
    /* Simulate PMWIN's native modal loop idle hook.  The worker was created
     * immediately before WinDlgBox: first yield lets it enter DosSleep, the
     * second (after its deadline) lets it finish while TID 1 is still parked
     * inside this provider call. */
    assert(pb->scheduler_yield);pb->scheduler_yield(pb->opaque);sleep_ms(2);pb->scheduler_yield(pb->opaque);assert(find_thread(r,2)&&find_thread(r,2)->state==THREAD_DEAD);
    fn(ND,0x3b,0,0xfacade);assert(dismissed==2);return 0x1234002a;}
static uint32_t f_dismiss(uintptr_t w,uintptr_t code){assert((w==ND&&code==42)||(w==ND2&&code==43));dismissed++;return 1;}
static uint32_t f_setshort(uintptr_t w,uintptr_t id,uintptr_t val,uintptr_t sign){assert(w==ND&&id==10&&val==123&&!sign);shorts++;return 1;}
static uint32_t f_getshort(uintptr_t w,uintptr_t id,uintptr_t out,uintptr_t sign){assert(w==ND&&id==10&&!sign);hp(out,2);*(uint16_t *)out=123;shorts++;return 1;}
static uint32_t f_control(uintptr_t w,uintptr_t id,uintptr_t msg,uintptr_t a,uintptr_t b){assert(w==ND&&id==10&&msg==0x143&&a==8&&!b);return 1;}
static uint32_t f_destroy(uintptr_t w){assert(w==NW);subproc(NW,2,0,0);return 1;}
static void *pm_get(void *o,uint32_t ord){(void)o;switch(ord){
#define P(o,f) case o:return (void *)(uintptr_t)f
    P(763,f_init);P(716,f_queue);P(909,f_create);P(929,f_subclass);P(822,f_pointer);P(838,f_process);P(757,f_ps);P(730,f_draw);P(849,f_scroll);P(923,f_dlg);P(729,f_dismiss);P(858,f_setshort);P(814,f_getshort);P(903,f_control);P(728,f_destroy);
    default:return NULL;}}
void soft386_test_pm_provider(struct Soft386PmBridge *b){struct Runtime *r=b->opaque;pb=b;soft386_pm_set_provider(b,0,NULL,pm_get);r->queue.module.loaded=1;r->queue.module.get_proc=qprovider;}
int main(int argc,char **argv){int rc=soft386_vessel_main(argc,argv);if(rc)return rc;
    if(qcalls){assert(!qcount&&qcalls>=5);puts("Soft386 R5B real CPU queue provider PASS");}
    else{assert(subclassed==1&&queried==1&&drawn==1&&scrolled==1&&dialogs==2&&shorts==2&&dismissed==2&&oldcalls==5);puts("Soft386 R5B real CPU PM provider PASS: callable old proc, native icon callback round trip, paint/scroll, pointer bitmap, nested modal callbacks, yielding, FS/x87/DF/registers");}return 0;}
