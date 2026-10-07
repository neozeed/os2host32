#define SOFT386_TEST_PROVIDER
#define main soft386_vessel_main
#include "../src/soft386_os2.c"
#undef main
#include "os2_net.h"
#include <assert.h>
#ifdef _WIN32
static volatile LONG sent,received;
#define GET(p) InterlockedCompareExchange(&(p),0,0)
#define SET(p,v) InterlockedExchange(&(p),(v))
#else
#include <stdatomic.h>
static atomic_int sent,received;
#define GET(p) atomic_load(&(p))
#define SET(p,v) atomic_store(&(p),(v))
#endif
static int pm_entered,searches;static char payload[8];
static int f_init(void){return 0;}
static int f_socket(int a,int b,int c){assert(a==2&&b==1&&!c);return 42;}
static int f_connect(int s,const void *a,int n){assert(s==42&&n==16&&!memcmp(a,"\2\0\x5a\xbe\x7f\0\0\1",8));return 0;}
static int f_send(int s,const void *p,int n,int f){assert(s==42&&n==8&&!f);memcpy(payload,p,8);SET(sent,1);return 8;}
static int f_recv(int s,void *p,int n,int f){unsigned i;assert(s==42&&n==8&&!f);for(i=0;i<5000&&!GET(sent);i++)sleep_ms(1);assert(GET(sent));memcpy(p,payload,8);SET(received,1);return 8;}
static int f_close(int s){assert(s==42);return 0;}
static int f_error(void){return 0;}
static void f_set(int e){(void)e;}
static Os2NetHost *f_host(const char *name){static char ip[]={127,0,0,1};static char *addresses[]={ip,NULL},*aliases[]={NULL};static Os2NetHost h={"localhost",aliases,2,4,addresses};assert(!strcmp(name,"localhost"));return &h;}
static uint32_t f_search(uint32_t flags,const char *path,const char *file,char *out,uint32_t cb){assert(flags==0&&strcmp(path,"PATH")&&*path&&(!strcmp(file,"hello.txt")||!strcmp(file,"exit.done"))&&cb==128);strcpy(out,"C:\\guest\\hello.txt");searches++;return 0;}
static uint32_t f_pm(uintptr_t arg){assert(!arg);pm_entered++;return 0;}
static void *so_get(void *o,uint32_t n){(void)o;switch(n){
#define P(n,f) case n:return (void *)(uintptr_t)f
 P(26,f_init);P(16,f_socket);P(3,f_connect);P(13,f_send);P(10,f_recv);P(17,f_close);P(20,f_error);P(35,f_set);default:return NULL;}}
static void *tcp_get(void *o,uint32_t n){(void)o;switch(n){P(11,f_host);P(51,f_error);default:return NULL;}}
static void *dos_get(void *o,uint32_t n){(void)o;switch(n){P(228,f_search);default:return NULL;}}
static void *pm_get(void *o,uint32_t n){(void)o;switch(n){P(763,f_pm);default:return NULL;}}
#undef P
void soft386_test_pm_provider(struct Soft386PmBridge *b){struct Runtime *r=b->opaque;
 soft386_doscalls_bridge_set_provider(&r->net.modules[0],NULL,so_get,NULL,0);soft386_doscalls_bridge_set_provider(&r->net.modules[1],NULL,tcp_get,NULL,0);
 soft386_doscalls_bridge_set_provider(&r->native_dos,NULL,dos_get,NULL,0);
 /* Keep the injected DOS provider: production defaults still open their DLL. */
 r->native_dos_disabled=1;soft386_pm_set_provider(b,0,NULL,pm_get);
}
static unsigned boundary_searches;
static uint32_t isolated_search(uint32_t flags,const char *path,const char *file,char *out,uint32_t cap){
 assert(flags==0&&!strcmp(path,"JAR_ONLY")&&!strcmp(file,"x"));boundary_searches++;
 if(cap<12)return 111;memcpy(out,"JAR_ONLY/x",11);return 0;
}
static void *isolated_provider(void *p,uint32_t o){(void)p;return o==228?(void *)(uintptr_t)isolated_search:NULL;}
static void search_boundaries(void){
 struct Runtime *r=calloc(1,sizeof(*r));assert(r);r->ram=calloc(1,RAM_SIZE);assert(r->ram);
 soft386_doscalls_bridge_set_provider(&r->native_dos,NULL,isolated_provider,NULL,0);
 memcpy(r->ram+0x1000,"PATH=JAR_ONLY\0\0",15);strcpy((char *)r->ram+0x2000,"PATH");strcpy((char *)r->ram+0x2100,"x");guest_put_u32(r,GUEST_INFO+16,0x1000);
 assert(!guest_search_path(r,2,0x2000,0x2100,0x2200,64)&&!strcmp((char *)r->ram+0x2200,"JAR_ONLY/x"));
 memset(r->ram+0x2200,0xcc,64);assert(guest_search_path(r,2,0x2000,0x2100,0x2200,2)==111&&r->ram[0x2200]==0xcc);
 strcpy((char *)r->ram+0x2000,"MISSING");assert(guest_search_path(r,2,0x2000,0x2100,0x2200,64)==203);
 assert(guest_search_path(r,0,0x2000,0x2100,RAM_SIZE-4,64)==87&&boundary_searches==2);
 assert(guest_search_path(r,8,0,0x2100,0x2200,64)==87);free(r->ram);free(r);
}
int main(int argc,char **argv){int rc;search_boundaries();rc=soft386_vessel_main(argc,argv);assert(!rc);if(GET(received))puts("R5C Tiny386 async network PASS: worker progress, guest buffers, FS/x87/EBX preserved");else if(searches){assert(searches==2);puts("R5C Tiny386 search + ordered exit callback PASS");}else{assert(pm_entered==1);puts("R5C exact TELNETPM CRT-to-WinInitialize + real exit-list callback PASS (headless refusal, not GUI acceptance)");}return 0;}
