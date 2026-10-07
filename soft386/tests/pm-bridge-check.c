/* Boundary regressions exercise real marshal code against adversarial guest
 * buffers and provider-visible host pointers. No display server is needed. */
#include "soft386_pm_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static unsigned char ram[16384];
static unsigned calls,dispatches;static uint32_t current_tid=1;
static int valid(void *o,uint32_t p,uint32_t n,int w){(void)o;(void)w;return p&&p<sizeof(ram)&&n<=sizeof(ram)-p;}
static int readmem(void *o,uint32_t p,void *b,uint32_t n){if(!valid(o,p,n,0))return 0;memcpy(b,ram+p,n);return 1;}
static int writemem(void *o,uint32_t p,const void *b,uint32_t n){if(!valid(o,p,n,1))return 0;memcpy(ram+p,b,n);return 1;}
static int readstr(void *o,uint32_t p,char *out,uint32_t cap){uint32_t i;for(i=0;i<cap;i++){if(!readmem(o,p+i,out+i,1))return 0;if(!out[i])return 1;}return 0;}
static uint32_t u32(void *o,uint32_t p){uint32_t v=0;readmem(o,p,&v,4);return v;}
static void p32(uint32_t p,uint32_t v){assert(writemem(NULL,p,&v,4));}
static uint32_t tid(void *o){(void)o;return current_tid;}
static uint32_t hostcall_tid;
static int in_hostcall(void *o,uint32_t t){(void)o;return t==hostcall_tid;}
static void hostptr(uintptr_t p,size_t n){assert(p&&p>sizeof(ram));assert(p+n<=(uintptr_t)ram||p>=(uintptr_t)ram+sizeof(ram));}
static uint32_t init(uintptr_t x){assert(!x);calls++;return 1;}
static uint32_t queue(uintptr_t h,uintptr_t n){assert(h==1&&!n);calls++;return 0xe1111111;}
static uint32_t destroyqueue(uintptr_t q){(void)q;calls++;return 1;}
static uint32_t terminate(uintptr_t h){assert(h==1);calls++;return 1;}
static uint32_t createps(uintptr_t a,uintptr_t d,uintptr_t z,uintptr_t o){(void)o;assert(a==1&&!d);hostptr(z,8);calls++;return 0xe2222222;}
static uint32_t rect(uintptr_t w,uintptr_t r){uint32_t *p=(void *)r;assert(w==1);hostptr(r,16);p[0]=1;p[1]=2;p[2]=3;p[3]=4;calls++;return 1;}
static uint32_t createregion(uintptr_t ps,uintptr_t count,uintptr_t rects){assert(ps==0xe2222222&&!count&&!rects);calls++;return 0xe1515151;}
static uint32_t queryupdateregion(uintptr_t w,uintptr_t r){assert(w==1&&r==0xe1515151);calls++;return 2;}
static uint32_t bitmap(uintptr_t ps,uintptr_t hdr,uintptr_t flags,uintptr_t bits,uintptr_t info){const unsigned char *p=(void *)hdr;(void)flags;assert(ps==0xe2222222);hostptr(hdr,12);hostptr(bits,8);hostptr(info,780);assert(p[0]==12&&p[4]==2&&p[6]==2&&p[10]==8);assert(!memcmp((void *)bits,"12345678",8));calls++;return 0xe3333333;}
static uint32_t setbitmap(uintptr_t ps,uintptr_t bitmap){assert(ps==0xe2222222&&bitmap==0xe3333333);calls++;return 0;}
static uint32_t bitblt(uintptr_t dst,uintptr_t src,uintptr_t count,uintptr_t pts,uintptr_t rop,uintptr_t opt){
    assert(dst==0xe2222222&&!src&&count==2&&rop==0xff&&opt==2);hostptr(pts,16);calls++;return 1;
}
static uint32_t getbits(uintptr_t ps,uintptr_t start,uintptr_t count,uintptr_t bits,uintptr_t info){assert(ps==0xe2222222&&!start&&count==2);hostptr(bits,8);hostptr(info,780);memcpy((void *)bits,"abcdefgh",8);((unsigned char *)info)[12]=0x7a;calls++;return 2;}
static uint32_t insert(uintptr_t table,uintptr_t count,uintptr_t input,uintptr_t len,uintptr_t out,uintptr_t cap,uintptr_t actual){char **t=(void *)table;assert(count==1&&len==2&&cap==16);hostptr(table,sizeof(char *));hostptr((uintptr_t)t[0],4);assert(!strcmp(t[0],"jar"));hostptr(input,len);assert(!memcmp((void *)input,"%1",2));hostptr(out,cap);hostptr(actual,4);memcpy((void *)out,"jar",3);*(uint32_t *)actual=3;calls++;return 0;}
static uint32_t peek(uintptr_t hab,uintptr_t q,uintptr_t w,uintptr_t f,uintptr_t l,uintptr_t flags){uint32_t *p=(void *)q;assert(hab==1&&!w&&!f&&!l&&flags==1);hostptr(q,28);memset(p,0,28);p[0]=0xe4444444;p[1]=0xf;p[2]=0xc001beef;p[3]=0xf001beef;p[4]=123;calls++;return 1;}
static uint32_t dispatch(uintptr_t hab,uintptr_t q){uint32_t *p=(void *)q;assert(hab==1);hostptr(q,28);assert(p[0]==0xe4444444&&p[1]==0xf&&p[2]==0xc001beef&&p[3]==0xf001beef);dispatches++;return 42;}
static uint32_t postmsg(uintptr_t w,uintptr_t msg,uintptr_t mp1,uintptr_t mp2){assert(w==0xe8888888&&msg==0x20&&mp1==0x10cb&&!mp2);calls++;return 1;}
static uint32_t sendmsg(uintptr_t w,uintptr_t msg,uintptr_t mp1,uintptr_t mp2){
    if(msg==0x160){assert(w==1&&!mp1&&!mp2);calls++;return 37;}
    if(msg==0x182){uint8_t *mi=(void *)mp2;assert(w==0xe1212121&&mp1==12);hostptr(mp2,16);memset(mi,0,16);*(uint16_t *)(mi+6)=12;*(uint32_t *)(mi+8)=0xe1313131;calls++;return 1;}
    if(msg==0x192){assert(w==0xe1313131&&mp1==0x1000d&&mp2==0x40004000);calls++;return 1;}
    assert(0);return 0;
}
static uint32_t senddlg(uintptr_t w,uintptr_t id,uintptr_t msg,uintptr_t mp1,uintptr_t mp2){assert(w==1&&id==99&&msg==0x164&&mp1==3&&mp2==1);calls++;return 73;}
static uint32_t untyped_export(void){calls++;return 0x55aa55aa;}
static uint32_t windowfromid(uintptr_t w,uintptr_t id){assert(w==1&&id==0x8005);calls++;return 0xe1212121;}
static uintptr_t errorinfo(uintptr_t hab){assert(hab==1);calls++;return 0;}
static uint32_t devopendc(uintptr_t hab,uintptr_t type,uintptr_t token,uintptr_t count,uintptr_t data,uintptr_t compat){uintptr_t *v=(void *)data;assert(hab==1&&type==8&&count==3&&!compat);hostptr(token,2);assert(!strcmp((char *)token,"*"));hostptr(data,3*sizeof(uintptr_t));assert(!v[0]&&v[1]&&!v[2]);hostptr(v[1],8);assert(!strcmp((char *)v[1],"DISPLAY"));calls++;return 0xe1414141;}

static uintptr_t control_proc;
static uint32_t newwindow(uintptr_t par,uintptr_t cls,uintptr_t text,uintptr_t style,uintptr_t x,uintptr_t y,uintptr_t cx,uintptr_t cy,uintptr_t own,uintptr_t after,uintptr_t id,uintptr_t ctl,uintptr_t pres){assert(par==1&&cls==0xffff0005u&&!text&&!style&&x==2&&y==3&&cx==100&&cy==200&&!own&&after==4&&id==42&&!ctl&&!pres);calls++;return 0xe8888888;}
static uint32_t oldcontrol(uintptr_t w,uintptr_t msg,uintptr_t a,uintptr_t c){assert(w==0xe8888888&&(msg==0x101||msg==0x23)&&!a&&!c);calls++;return msg==0x101?0xe9999999:0;}
static uintptr_t subclass(uintptr_t w,uintptr_t fn){uintptr_t old=control_proc;assert(w==0xe8888888&&fn);calls++;if(old)return old;control_proc=fn;return (uintptr_t)oldcontrol;}
static uint32_t pointerinfo(uintptr_t p,uintptr_t out){uint32_t *v=(void *)out;assert(p==0xe9999999);hostptr(out,28);memset(v,0,28);v[0]=1;v[1]=4;v[2]=5;v[3]=0xe3333333;v[4]=0xeaaaaaaa;calls++;return 1;}
static uint32_t windowprocess(uintptr_t w,uintptr_t pid,uintptr_t thread){assert(w==0xe8888888);if(pid){hostptr(pid,4);*(uint32_t *)pid=12345;}if(thread){hostptr(thread,4);*(uint32_t *)thread=999;}calls++;return 1;}
static uint32_t drawbitmap(uintptr_t ps,uintptr_t bm,uintptr_t src,uintptr_t dest,uintptr_t fore,uintptr_t back,uintptr_t flags){assert(ps==0xe2222222&&bm==0xe3333333&&!src&&!fore&&!back&&(flags==0||flags==4));hostptr(dest,flags?16:8);calls++;return 1;}
static uint32_t scrollwindow(uintptr_t w,uintptr_t dx,uintptr_t dy,uintptr_t sr,uintptr_t cr,uintptr_t rgn,uintptr_t out,uintptr_t flags){assert(w==0xe8888888&&dx==3&&dy==7&&!cr&&!rgn&&flags==2);if(sr){hostptr(sr,16);assert(*(uint32_t *)sr==11);}if(out){hostptr(out,16);memset((void *)out,0,16);*(uint32_t *)out=22;}calls++;return 2;}
static uint32_t shortout(uintptr_t w,uintptr_t id,uintptr_t out,uintptr_t sign){assert(w==0xe8888888&&id==10&&sign==1);hostptr(out,2);*(uint16_t *)out=0xfff9;calls++;return 1;}
static uint32_t destroywindow(uintptr_t w){(void)w;calls++;return 1;}
static uint32_t veneer(void *o,uint32_t slot){(void)o;return 0xf0000+slot*8;}
static int code_address(void *o,uint32_t p){(void)o;return p==0x1234||p==0x5678;}
static unsigned invoked;static uint32_t cb_args[4],cb_result;
static int invoke(void *o,uint32_t t,uint32_t p,const uint32_t *a,uint32_t n,uint32_t *r){(void)o;assert(t==1&&p==0x1234&&n==4);memcpy(cb_args,a,sizeof(cb_args));invoked++;*r=cb_result;return 1;}

static uint32_t loadaccel(uintptr_t h,uintptr_t mod,uintptr_t id){assert(h==1&&!mod&&id==100);calls++;return 0xeeee1234;}
static uint32_t setaccel(uintptr_t h,uintptr_t accel,uintptr_t w){assert(h==1&&accel==0xeeee1234&&w==1);calls++;return 1;}
static uint32_t cursorinfo(uintptr_t w,uintptr_t info){assert(w==1);hostptr(info,40);memset((void *)info,0,40);*(uint32_t *)info=1;calls++;return 1;}
static uint32_t trackrect(uintptr_t w,uintptr_t ps,uintptr_t info){assert(w==1&&!ps);hostptr(info,76);calls++;return 1;}
static uint32_t loadstring(uintptr_t hab,uintptr_t module,uintptr_t id,uintptr_t cap,uintptr_t out){
    const char *v=id==100?"Help":"Msg";size_t n=strlen(v);
    assert(hab==1&&!module&&cap>=n+1);hostptr(out,(size_t)cap);
    memcpy((void *)out,v,n+1);calls++;return (uint32_t)n;
}
static uint32_t queryfonts(uintptr_t ps,uintptr_t options,uintptr_t face,uintptr_t count,uintptr_t stride,uintptr_t metrics){
    assert(ps==0xe2222222&&options==1&&face==0);hostptr(count,4);calls++;
    if(!*(uint32_t *)count){assert(!metrics);return 2;}
    assert(*(uint32_t *)count==2&&stride==228);hostptr(metrics,456);
    memset((void *)metrics,42,456);*(uint32_t *)count=2;return 0;
}
static void *pm(void *o,uint32_t n){(void)o;switch(n){case 779:case 781:return (void *)loadstring;case 776:return (void *)loadaccel;case 850:return (void *)setaccel;case 812:return (void *)cursorinfo;case 890:return (void *)trackrect;case 909:return (void *)newwindow;case 929:return (void *)subclass;case 822:return (void *)pointerinfo;case 838:return (void *)windowprocess;case 730:return (void *)drawbitmap;case 849:return (void *)scrollwindow;case 814:return (void *)shortout;case 728:return (void *)destroywindow;case 763:return (void *)init;case 716:return (void *)queue;case 726:return (void *)destroyqueue;case 888:return (void *)terminate;case 840:return (void *)rect;case 832:return (void *)queryupdateregion;case 918:return (void *)peek;case 912:return (void *)dispatch;case 919:return (void *)postmsg;case 920:return (void *)sendmsg;case 903:return (void *)senddlg;case 899:return (void *)windowfromid;case 751:return (void *)errorinfo;case 999:return (void *)untyped_export;default:return NULL;}}
static void *gpi(void *o,uint32_t n){(void)o;switch(n){case 355:return (void *)bitblt;case 610:return (void *)devopendc;case 370:return (void *)createregion;case 586:return (void *)queryfonts;case 369:return (void *)createps;case 598:return (void *)bitmap;case 506:return (void *)setbitmap;case 599:return (void *)getbits;default:return NULL;}}
static void *msg(void *o,uint32_t n){(void)o;return n==4?(void *)insert:NULL;}
static uint32_t openprofile(uintptr_t hab,uintptr_t file){assert(hab==1);hostptr(file,8);assert(!strcmp((char *)file,"jar.ini"));calls++;return 0xe5555555;}
static uint32_t closeprofile(uintptr_t hini){assert(hini==0xe5555555);calls++;return 1;}
static void profile_args(uintptr_t hini,uintptr_t app,uintptr_t key){assert(hini==0xe5555555||hini==0xffffffffu);hostptr(app,4);hostptr(key,4);assert(!strcmp((char *)app,"jar")&&!strcmp((char *)key,"key"));calls++;}
static uint32_t profileint(uintptr_t hini,uintptr_t app,uintptr_t key,uintptr_t def){profile_args(hini,app,key);assert(def==42);return 0xfff9;}
static uint32_t profilestring(uintptr_t hini,uintptr_t app,uintptr_t key,uintptr_t value){profile_args(hini,app,key);hostptr(value,6);assert(!strcmp((char *)value,"saved"));return 1;}
static uint32_t profiledata(uintptr_t hini,uintptr_t app,uintptr_t key,uintptr_t data,uintptr_t size){uint32_t *cb=(void *)size;profile_args(hini,app,key);hostptr(size,4);if(!data||*cb<4){*cb=4;return 0;}hostptr(data,*cb);memcpy((void *)data,"DATA",4);*cb=4;return 1;}
static uint32_t writeprofile(uintptr_t hini,uintptr_t app,uintptr_t key,uintptr_t data,uintptr_t size){profile_args(hini,app,key);hostptr(data,size);assert(size==4&&!memcmp((void *)data,"DATA",4));return 1;}
static uint32_t profilesize(uintptr_t hini,uintptr_t app,uintptr_t key,uintptr_t size){profile_args(hini,app,key);hostptr(size,4);*(uint32_t *)size=4;return 1;}
static uint32_t addswitch(uintptr_t data){uint32_t *p=(void *)data;hostptr(data,96);assert(p[0]==0xe4444444&&!p[1]&&!p[2]);calls++;return 0xe6666666;}
static uint32_t removeswitch(uintptr_t h){assert(h==0xe6666666);calls++;return 0;}
static void *profile(void *o,uint32_t n){(void)o;switch(n){case 101:return (void *)profilesize;case 102:return (void *)openprofile;case 103:return (void *)closeprofile;case 114:return (void *)profileint;case 116:return (void *)profilestring;case 117:return (void *)profiledata;case 118:return (void *)writeprofile;case 120:return (void *)addswitch;case 129:return (void *)removeswitch;default:return NULL;}}
struct TestHelpInit {uint32_t cb,rc;char *tutorial;void *table;uint32_t table_module,accel_module,accel,action;char *title;uint32_t show;char *library;};
static uint32_t createhelp(uintptr_t hab,uintptr_t data){struct TestHelpInit *p=(void *)data;assert(hab==1);hostptr(data,sizeof(*p));assert(p->cb==sizeof(*p)&&(uintptr_t)p->table==0xffff0001u);hostptr((uintptr_t)p->tutorial,4);hostptr((uintptr_t)p->title,4);hostptr((uintptr_t)p->library,4);assert(!strcmp(p->title,"jar"));p->rc=0;calls++;return 0xe7777777;}
static uint32_t associatehelp(uintptr_t help,uintptr_t win){assert(help==0xe7777777&&win==0xe4444444);calls++;return 1;}
static uint32_t destroyhelp(uintptr_t help){assert(help==0xe7777777);calls++;return 1;}
static void *help(void *o,uint32_t n){(void)o;switch(n){case 51:return (void *)createhelp;case 52:return (void *)destroyhelp;case 54:return (void *)associatehelp;default:return NULL;}}
static const struct Soft386GuestMemoryOps memory={NULL,valid,readmem,writemem,readstr,u32};
static uint32_t run(struct Soft386PmBridge *b,unsigned mod,uint32_t ord,const uint32_t *a,unsigned n){unsigned i;int handled=0;uint32_t result;memset(ram+256,0,64);for(i=0;i<n;i++)p32(260+4*i,a[i]);result=soft386_pm_dispatch(b,&memory,mod,ord,256,&handled);assert(handled);return result;}
#define RUN(mod,ord,...) run(&b,mod,ord,(uint32_t[]){__VA_ARGS__},sizeof((uint32_t[]){__VA_ARGS__})/4)
int main(void){struct Soft386PmBridge b;uint32_t hab,hps,hbm,n,saved[7],hmq,hini,hswitch,hhelp,hrgn,hab2,hmq2;soft386_pm_init(&b,NULL,tid,NULL);soft386_pm_set_thread_hostcall(&b,in_hostcall);soft386_pm_set_provider(&b,0,NULL,pm);soft386_pm_set_provider(&b,1,NULL,gpi);soft386_pm_set_provider(&b,3,NULL,msg);soft386_pm_set_provider(&b,S386_PMSHAPI,NULL,profile);soft386_pm_set_provider(&b,S386_HELPMGR,NULL,help);
    /* H2B: the provider can export an API even when the guest ABI crossing is not described. */
    {int handled=0;uint32_t before=calls;assert(soft386_pm_export(&b,0,999));assert(!soft386_pm_dispatch(&b,&(struct Soft386GuestMemoryOps){NULL,valid,readmem,writemem,readstr,u32},0,999,256,&handled));assert(handled&&calls==before);}
    hab=RUN(0,763,0);assert(hab&&hab!=1);hmq=RUN(0,716,hab,0);assert(hmq!=0xe1111111);
    assert(RUN(0,840,1,1024)==1&&u32(NULL,1024)==1&&u32(NULL,1036)==4);
    /* H2R: bounded PM string copyout must not zero the unused staging tail. */
    memset(ram+12000,0xa5,96);
    assert(RUN(0,781,hab,0,100,64,12000)==4);
    assert(!memcmp(ram+12000,"Help\0",5));
    {unsigned j;for(j=5;j<64;j++)assert(ram[12000+j]==0xa5);}
    memset(ram+12100,0x5a,64);
    assert(RUN(0,779,hab,0,101,32,12100)==3);
    assert(!memcmp(ram+12100,"Msg\0",4));
    {unsigned j;for(j=4;j<32;j++)assert(ram[12100+j]==0x5a);}
    /* Message semantics live in PMWIN.  The bridge only proves the MPARAM ABI
       is scalar/handle-safe, then forwards the call unchanged. */
    assert(RUN(0,920,1,0x160,0,0)==37);assert(RUN(0,903,1,99,0x164,3,1)==73);
    /* H2D: MM_QUERYITEM marshals its MENUITEM output and tokenizes the
       returned submenu before a later scalar MM_SETITEMATTR uses it. */
    {uint32_t menu=RUN(0,899,1,0x8005),submenu;assert(menu&&menu!=0xe1212121);memset(ram+15000,0xcc,16);
     assert(RUN(0,920,menu,0x182,12,15000)==1);submenu=u32(NULL,15008);assert(submenu&&submenu!=0xe1313131);
     assert(RUN(0,920,submenu,0x192,0x1000d,0x40004000)==1);}
    assert(!RUN(0,751,hab));
    strcpy((char *)ram+15100,"*");strcpy((char *)ram+15120,"DISPLAY");p32(15140,0);p32(15144,15120);p32(15148,0);
    {uint32_t dc=RUN(1,610,hab,8,15100,3,15140,0);assert(dc&&dc!=0xe1414141);}
    n=calls;assert(!RUN(0,920,1,0x7777,0,0)&&calls==n);
    n=calls;assert(!RUN(0,840,1,sizeof(ram)-8)&&calls==n);assert(!RUN(0,840,0xfeedface,1024)&&calls==n);
    hps=RUN(1,369,hab,0,1100,0);assert(hps&&hps!=0xe2222222);
    hrgn=RUN(1,370,hps,0,0);assert(hrgn&&hrgn!=0xe1515151);assert(RUN(0,832,1,hrgn)==2);
    p32(1120,1);p32(1124,2);p32(1128,30);p32(1132,40);
    assert(RUN(1,355,hps,0,2,1120,0xff,2)==1); /* NULL-source BitBlt uses two POINTLs */
    n=calls;assert(!RUN(0,840,hps,1024)&&calls==n); /* handle-kind confusion */
    p32(2048,12);ram[2052]=2;ram[2054]=2;ram[2056]=1;ram[2058]=8;
    memcpy(ram+3072,"12345678",8);memcpy(ram+4096,ram+2048,12);
    hbm=RUN(1,598,hps,2048,0,3072,4096);assert(hbm&&hbm!=0xe3333333);
    assert(!RUN(1,506,hps,hbm));assert(RUN(1,599,hps,0,2,3072,4096)==2);assert(!memcmp(ram+3072,"abcdefgh",8)&&ram[4108]==0x7a);
    n=calls;assert(!RUN(1,599,hps,0,0xffffffff,3072,4096)&&calls==n);assert(!RUN(1,599,hps,0,2,sizeof(ram)-4,4096)&&calls==n);
    strcpy((char *)ram+7000,"jar");p32(6000,7000);memcpy(ram+7200,"%1",2);assert(!RUN(3,4,6000,1,7200,2,7300,16,7400));assert(u32(NULL,7400)==3&&!memcmp(ram+7300,"jar",3));
    n=calls;p32(6000,sizeof(ram)-1);ram[sizeof(ram)-1]='x';assert(RUN(3,4,6000,1,7200,2,7300,16,7400)==87&&calls==n);
    assert(RUN(0,915,hab,8000,0,0,0)==1);memcpy(saved,ram+8000,28);assert((saved[1]&0xffff)==0x23&&!(saved[2]|saved[3])&&(saved[1]>>16));
    p32(8008,0xdeadbeef);assert(!RUN(0,912,hab,8000)&&!dispatches);memcpy(ram+8100,saved,28);assert(RUN(0,912,hab,8100)==42&&dispatches==1);assert(!RUN(0,912,hab,8100)&&dispatches==1);
    /* Queue messages retain guest MPARAMs as guest values, in FIFO order. */
    assert(RUN(0,902,hmq,0x1001,17,25));assert(RUN(0,902,hmq,0x1002,19,23));
    assert(RUN(0,915,hab,8200,0,0,0));assert((u32(NULL,8204)&0xffff)==0x1001&&u32(NULL,8208)==17);
    assert(!RUN(0,912,hab,8200));assert(RUN(0,902,hmq,0x1003,1,2));
    assert(RUN(0,915,hab,8200,0,0,0));assert((u32(NULL,8204)&0xffff)==0x1002);
    current_tid=2;hab2=RUN(0,763,0);assert(hab2&&hab2!=hab);n=calls;hmq2=RUN(0,716,hab2,150);assert(hmq2&&calls==n);
    current_tid=1;assert(RUN(0,902,hmq2,0x1100,0x44,0x55));
    current_tid=2;assert(RUN(0,915,hab2,8200,0,0,0));assert((u32(NULL,8204)&0xffff)==0x1100&&u32(NULL,8208)==0x44&&u32(NULL,8212)==0x55);assert(!RUN(0,912,hab2,8200));
    n=calls;assert(!RUN(0,915,hab,8200,0,0,0)&&calls==n);assert(RUN(0,726,hmq2));assert(RUN(0,888,hab2));
    current_tid=1;strcpy((char *)ram+9000,"jar.ini");strcpy((char *)ram+9040,"jar");strcpy((char *)ram+9080,"key");strcpy((char *)ram+9120,"saved");
    hini=RUN(S386_PMSHAPI,102,hab,9000);assert(hini&&hini!=0xe5555555);
    assert(RUN(S386_PMSHAPI,114,hini,9040,9080,42)==0xfffffff9u);
    assert(RUN(S386_PMSHAPI,114,0xffffffffu,9040,9080,42)==0xfffffff9u);
    assert(RUN(S386_PMSHAPI,116,hini,9040,9080,9120)==1);
    p32(9160,8);assert(RUN(S386_PMSHAPI,117,hini,9040,9080,9200,9160)==1&&u32(NULL,9160)==4&&!memcmp(ram+9200,"DATA",4));
    assert(RUN(S386_PMSHAPI,118,hini,9040,9080,9200,4)==1);
    p32(9160,0);assert(!RUN(S386_PMSHAPI,117,hini,9040,9080,0,9160)&&u32(NULL,9160)==4);
    assert(RUN(S386_PMSHAPI,101,hini,9040,9080,9160)&&u32(NULL,9160)==4);
    n=calls;p32(9160,64);assert(!RUN(S386_PMSHAPI,117,hini,9040,9080,sizeof(ram)-4,9160)&&calls==n);
    assert(!RUN(S386_PMSHAPI,114,hps,9040,9080,42)&&calls==n);
    assert(RUN(S386_PMSHAPI,103,hini));n=calls;assert(!RUN(S386_PMSHAPI,114,hini,9040,9080,42)&&calls==n);
    memset(ram+9300,0,96);p32(9300,saved[0]);hswitch=RUN(S386_PMSHAPI,120,9300);assert(hswitch&&hswitch!=0xe6666666&&u32(NULL,9300)==saved[0]);
    assert(!RUN(S386_PMSHAPI,129,hswitch));n=calls;assert(RUN(S386_PMSHAPI,129,hswitch)==87&&calls==n);
    memset(ram+9500,0,44);p32(9500,44);p32(9504,99);p32(9508,9040);p32(9512,0xffff0001u);p32(9532,9040);p32(9540,9080);
    hhelp=RUN(S386_HELPMGR,51,hab,9500);assert(hhelp&&hhelp!=0xe7777777&&!u32(NULL,9504)&&u32(NULL,9508)==9040&&u32(NULL,9512)==0xffff0001u);
    assert(RUN(S386_HELPMGR,54,hhelp,saved[0]));assert(RUN(S386_HELPMGR,52,hhelp));
    n=calls;assert(!RUN(S386_HELPMGR,54,hhelp,saved[0])&&calls==n);p32(9512,9040);assert(!RUN(S386_HELPMGR,51,hab,9500)&&calls==n);
    {
        uint32_t w,old,icon,slot,mbm;
        b.veneer=veneer;b.code_address=code_address;b.invoke=invoke;
        w=RUN(0,909,1,0xffff0005,0,0,2,3,100,200,0,4,42,0,0);assert(w);
        /* H2K: ordinary private posts stay jar-local; only a target guest
           thread parked in HOSTCALL/modal state bypasses to PMWIN. */
        hostcall_tid=0;n=calls;assert(!RUN(0,919,w,0x10cd,0,0)&&calls==n);
        hostcall_tid=current_tid;n=calls;assert(RUN(0,919,w,0x20,0x10cb,0)==1&&calls==n+1);hostcall_tid=0;
        n=calls;assert(!RUN(0,909,1,0xffff0005,0,0,2,3,100,200,0,4,42,0,100)&&calls==n);
        assert(!RUN(0,929,w,0xbadbeef)&&calls==n);
        old=RUN(0,929,w,0x1234);assert(old>=0xf0008&&old<0xf1000);slot=(old-0xf0000)/8;
        assert(RUN(0,929,w,0x5678)==0x1234); /* native compat returns its original callback */
        p32(260,w);p32(264,0x101);p32(268,0);p32(272,0);
        icon=soft386_pm_call_proc(&b,&memory,slot,256);assert(icon&&icon!=0xe9999999);
        {uint32_t (*cb)(uint32_t,uint32_t,uint32_t,uint32_t)=(void *)control_proc;unsigned before=invoked;
         cb_result=icon;
         assert(cb(0xe8888888,0x100,0xe9999999,0)==0xe9999999&&invoked==before+1);
         assert(cb_args[0]==w&&cb_args[1]==0x100&&cb_args[2]==icon&&!cb_args[3]);
         assert(cb(0xe8888888,0x101,0,0)==0xe9999999);
         cb_result=hps;assert(!cb(0xe8888888,0x101,0,0)); /* reject wrong result kind */
         cb_result=0xfacade;assert(!cb(0xe8888888,0x101,0,0)); /* raw pointer cannot escape */
         cb_result=0;assert(!cb(0xe8888888,0x100,0,0)&&!cb_args[2]);
         before=invoked;assert(!cb(0xe8888888,0x182,0,0xfeedface)&&invoked==before);
        }
        assert(RUN(0,822,icon,9600)==1&&u32(NULL,9604)==4&&u32(NULL,9608)==5);
        mbm=u32(NULL,9612);assert(mbm==hbm&&u32(NULL,9616)!=0xeaaaaaaa&&!u32(NULL,9620)&&!u32(NULL,9624));
        n=calls;assert(!RUN(0,822,hps,9600)&&calls==n);assert(!RUN(0,822,icon,sizeof(ram)-24)&&calls==n);
        assert(RUN(0,838,w,9632,9636)&&u32(NULL,9632)==1&&u32(NULL,9636)==1);
        n=calls;assert(!RUN(0,838,w,9632,sizeof(ram)-3)&&calls==n);
        assert(RUN(0,730,hps,mbm,0,sizeof(ram)-8,0,0,0));
        n=calls;assert(!RUN(0,730,hps,mbm,0,sizeof(ram)-8,0,0,4)&&calls==n);
        assert(RUN(0,730,hps,mbm,0,sizeof(ram)-16,0,0,4));
        p32(9664,11);assert(RUN(0,849,w,3,7,9664,0,0,9680,2)==2&&u32(NULL,9664)==11&&u32(NULL,9680)==22);
        n=calls;assert(!RUN(0,849,w,3,7,0,0,0,sizeof(ram)-12,2)&&calls==n);
        assert(!RUN(0,849,w,3,7,0,0,hps,9680,2)&&calls==n);
        p32(9700,0xcccccccc);assert(RUN(0,814,w,10,9700,1)&&u32(NULL,9700)==0xccccfff9);
        n=calls;assert(!RUN(0,814,w,10,sizeof(ram)-1,1)&&calls==n);
        p32(260,w);p32(264,0x101);p32(268,0);p32(272,0);current_tid=2;
        assert(!soft386_pm_call_proc(&b,&memory,slot,256)&&calls==n);
        assert(!RUN(0,929,w,0x1234)&&calls==n);current_tid=1;
        p32(260,w);p32(264,0x182);p32(268,0);p32(272,9600);
        assert(!soft386_pm_call_proc(&b,&memory,slot,256)&&calls==n);
        assert(RUN(0,728,w));n=calls;p32(260,w);p32(264,0x101);p32(268,0);p32(272,0);
        assert(!soft386_pm_call_proc(&b,&memory,slot,256)&&calls==n);
    }
    {uint32_t accel=RUN(0,776,hab,0,100);assert(accel&&accel!=0xeeee1234);
     assert(RUN(0,850,hab,accel,1));n=calls;assert(!RUN(0,850,hab,hps,1)&&calls==n);
     assert(RUN(0,812,1,10000)&&u32(NULL,10000)==1);n=calls;assert(!RUN(0,812,1,sizeof(ram)-39)&&calls==n);
     assert(RUN(0,890,1,0,10080));n=calls;assert(!RUN(0,890,1,0,sizeof(ram)-75)&&calls==n);
     /* Exact TELNETPM sequence: count-only query, then 228-byte metrics. */
     p32(10200,0);n=calls;assert(RUN(1,586,hps,1,0,10200,0,0)==2&&calls==n+1&&!u32(NULL,10200));
     /* Unused metrics is ignored, never validated/dereferenced by the provider. */
     assert(RUN(1,586,hps,1,0,10200,0,0xfffffff0)==2);
     p32(10200,2);assert(RUN(1,586,hps,1,0,10200,228,10300)==0&&ram[10300]==42&&ram[10755]==42);
     n=calls;
     assert(RUN(1,586,hps,1,0,10200,0,10300)==UINT32_MAX&&calls==n);
     assert(RUN(1,586,hps,1,0,10200,228,0)==UINT32_MAX&&calls==n);
     assert(RUN(1,586,hps,1,0,10200,228,sizeof(ram)-455)==UINT32_MAX&&calls==n);
     assert(RUN(1,586,hps,1,0,10200,0xffffffff,10300)==UINT32_MAX&&calls==n);
     p32(10200,0xffffffff);assert(RUN(1,586,hps,1,0,10200,228,10300)==UINT32_MAX&&calls==n);
     assert(RUN(1,586,hps,1,0,0,228,10300)==UINT32_MAX&&calls==n);}

    soft386_pm_close(&b);puts("Soft386 I386-H2S PM ABI/scheduler PASS: message semantics delegated, zero-capacity fonts, callback icon tokens, 13-argument create, old-proc lifetime, pointer bitmaps, scroll rectangles, short outputs, bounds, typed handles, nested strings, bitmap buffers, QMSG integrity, FIFO, ownership, profiles, switch entries, help lifetime");return 0;
}
