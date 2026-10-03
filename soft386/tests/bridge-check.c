#include "soft386_doscalls_bridge.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef __cdecl
#define __cdecl
#endif
static uint8_t ram[65536];
static char opened[128];
static char wrote[128];
static char exec_program[128];
static char exec_argv0[128];
static char exec_tail[128];
static char exec_env0[128];

static uint32_t r32(uint32_t a){ return (uint32_t)ram[a]|((uint32_t)ram[a+1]<<8)|((uint32_t)ram[a+2]<<16)|((uint32_t)ram[a+3]<<24); }
static void w32(uint32_t a,uint32_t v){ram[a]=(uint8_t)v;ram[a+1]=(uint8_t)(v>>8);ram[a+2]=(uint8_t)(v>>16);ram[a+3]=(uint8_t)(v>>24);}
static int valid(void*o,uint32_t a,uint32_t n,int w){(void)o;(void)w;return a<sizeof(ram)&&n<=sizeof(ram)-a;}
static int rd(void*o,uint32_t a,void*d,uint32_t n){(void)o;if(!valid(0,a,n,0))return 0;memcpy(d,ram+a,n);return 1;}
static int wr(void*o,uint32_t a,const void*s,uint32_t n){(void)o;if(!valid(0,a,n,1))return 0;memcpy(ram+a,s,n);return 1;}
static int cstr(void*o,uint32_t a,char*d,uint32_t c){uint32_t i;(void)o;if(!a||!c)return 0;for(i=0;i<c;i++){if(!valid(0,a+i,1,0))return 0;d[i]=(char)ram[a+i];if(!d[i])return 1;}return 0;}
static uint32_t ru32(void*o,uint32_t a){(void)o;return r32(a);}

static uint32_t __cdecl fake_open(const char *p,uint32_t *hf,uint32_t *act,uint32_t cb,uint32_t attr,uint32_t flags,uint32_t mode,void *ea,uint32_t res)
{(void)cb;(void)attr;(void)flags;(void)mode;(void)res;assert(ea==0);strcpy(opened,p);*hf=42;*act=1;return 0;}
static uint32_t __cdecl fake_write(uint32_t h,const void *b,uint32_t n,uint32_t *actual)
{assert(h==42);assert(n<sizeof(wrote));memcpy(wrote,b,n);wrote[n]=0;*actual=n;return 0;}
static uint32_t __cdecl fake_read(uint32_t h,void *b,uint32_t n,uint32_t *actual)
{const char *s="xyz";assert(h==42);assert(n>=3);memcpy(b,s,3);*actual=3;return 0;}
static uint32_t __cdecl fake_disk(uint32_t *d,uint32_t *m){*d=3;*m=0x7ffffff;return 0;}
static uint32_t __cdecl fake_htype(uint32_t h,uint32_t*t,uint32_t*a){assert(h==42);*t=7;*a=9;return 0;}
struct FakeResultCodes{uint32_t term,result;};
static uint32_t __cdecl fake_exec(char *object,int32_t cb,uint32_t flag,const char *args,const char *env,struct FakeResultCodes *r,const char *program)
{
    const char *tail;
    assert(cb==64);assert(flag==2);assert(object&&r&&program);
    strcpy(exec_program,program);
    strcpy(exec_argv0,args?args:"");
    tail=args?args+strlen(args)+1:NULL;strcpy(exec_tail,(tail&&*tail)?tail:"");
    strcpy(exec_env0,env?env:"");
    strcpy(object,"");r->term=1234;r->result=0;return 0;
}
static uint32_t __cdecl fake_wait(uint32_t action,uint32_t option,struct FakeResultCodes*r,uint32_t *pid,uint32_t wanted)
{assert(action==0&&option==0&&wanted==1234&&r&&pid);r->term=0;r->result=7;*pid=1234;return 0;}
static void *getp(void *o,uint32_t ord){(void)o;switch(ord){case 273:return (void*)(uintptr_t)fake_open;case 282:return(void*)(uintptr_t)fake_write;case 281:return(void*)(uintptr_t)fake_read;case 275:return(void*)(uintptr_t)fake_disk;case 224:return(void*)(uintptr_t)fake_htype;case 283:return(void*)(uintptr_t)fake_exec;case 280:return(void*)(uintptr_t)fake_wait;default:return 0;}}
static void arg(unsigned n,uint32_t v){w32(0x1000+4+4*n,v);}

int main(void)
{
 struct Soft386NativeDoscalls b;struct Soft386GuestMemoryOps m;int h;uint32_t rc;
 static const uint32_t jar_owned[] = {
     229,234,299,304,305,306,311,312,318,319,320,321,322,
     324,325,326,327,328,329,330,331,332,333,334,335,336,
     344,345,346,347,348,349,354,355,378,418,425,426
 };
 unsigned i;
 for(i=0;i<sizeof(jar_owned)/sizeof(jar_owned[0]);++i)
     assert(!soft386_doscalls_bridge_ordinal(jar_owned[i]));
 assert(soft386_doscalls_bridge_ordinal(273));
 assert(soft386_doscalls_bridge_ordinal(281));
 assert(soft386_doscalls_bridge_ordinal(282));
 assert(soft386_doscalls_bridge_ordinal(280));
 assert(soft386_doscalls_bridge_ordinal(283));
 assert(soft386_doscalls_bridge_ordinal(382));
 memset(ram,0,sizeof(ram));memset(&m,0,sizeof(m));m.valid=valid;m.read=rd;m.write=wr;m.read_cstr=cstr;m.read_u32=ru32;
 soft386_doscalls_bridge_set_provider(&b,0,getp,0,0);
 strcpy((char*)ram+0x2000,"bridge.tmp");arg(0,0x2000);arg(1,0x2100);arg(2,0x2104);arg(3,0);arg(4,0);arg(5,1);arg(6,0x42);arg(7,0);arg(8,0);
 rc=soft386_doscalls_bridge_dispatch(&b,&m,273,0x1000,&h);assert(h&&rc==0&&r32(0x2100)==42&&r32(0x2104)==1&&!strcmp(opened,"bridge.tmp"));
 strcpy((char*)ram+0x2200,"abc");arg(0,42);arg(1,0x2200);arg(2,3);arg(3,0x2300);rc=soft386_doscalls_bridge_dispatch(&b,&m,282,0x1000,&h);assert(rc==0&&r32(0x2300)==3&&!strcmp(wrote,"abc"));
 memset(ram+0x2400,0,8);arg(0,42);arg(1,0x2400);arg(2,8);arg(3,0x2300);rc=soft386_doscalls_bridge_dispatch(&b,&m,281,0x1000,&h);assert(rc==0&&r32(0x2300)==3&&!memcmp(ram+0x2400,"xyz",3));
 arg(0,0x2500);arg(1,0x2504);rc=soft386_doscalls_bridge_dispatch(&b,&m,275,0x1000,&h);assert(rc==0&&r32(0x2500)==3&&r32(0x2504)==0x7ffffff);
 arg(0,42);arg(1,0x2510);arg(2,0x2514);rc=soft386_doscalls_bridge_dispatch(&b,&m,224,0x1000,&h);assert(rc==0&&r32(0x2510)==7&&r32(0x2514)==9);
 memset(ram+0x2600,0,64);memcpy(ram+0x2700,"typed\0-a 1\0\0",12);memcpy(ram+0x2800,"FOO=BAR\0X=Y\0\0",13);strcpy((char*)ram+0x2900,"child.exe");
 arg(0,0x2600);arg(1,64);arg(2,2);arg(3,0x2700);arg(4,0x2800);arg(5,0x2a00);arg(6,0x2900);
 rc=soft386_doscalls_bridge_dispatch(&b,&m,283,0x1000,&h);assert(h&&rc==0&&r32(0x2a00)==1234&&r32(0x2a04)==0&&!strcmp(exec_program,"child.exe")&&!strcmp(exec_argv0,"typed")&&!strcmp(exec_tail,"-a 1")&&!strcmp(exec_env0,"FOO=BAR"));
 arg(0,0);arg(1,0);arg(2,0x2b00);arg(3,0x2b08);arg(4,1234);rc=soft386_doscalls_bridge_dispatch(&b,&m,280,0x1000,&h);assert(h&&rc==0&&r32(0x2b00)==0&&r32(0x2b04)==7&&r32(0x2b08)==1234);
 puts("Soft386 DOSCALLS bridge marshalling: PASS");return 0;
}
