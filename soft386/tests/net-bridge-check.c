/* POSIX native test provider uses a real loopback TCP connection. */
#include "soft386_net_bridge.h"
#include "os2_net.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdatomic.h>
static unsigned char ram[0x80000];
static _Thread_local int err;
static atomic_int recv_entered,recv_leave;
static int calls;
static struct Soft386NetBridge b;
static uint32_t rd32(uint32_t p){uint32_t v;memcpy(&v,ram+p,4);return v;}
static void w32(uint32_t p,uint32_t v){memcpy(ram+p,&v,4);}
static int valid(void *o,uint32_t p,uint32_t n,int w){(void)o;(void)w;return p<sizeof(ram)&&n<=sizeof(ram)-p;}
static int readmem(void *o,uint32_t p,void *d,uint32_t n){if(!valid(o,p,n,0))return 0;memcpy(d,ram+p,n);return 1;}
static int writemem(void *o,uint32_t p,const void *d,uint32_t n){if(!valid(o,p,n,1))return 0;memcpy(ram+p,d,n);return 1;}
static int readstr(void *o,uint32_t p,char *s,uint32_t n){unsigned i;for(i=0;i<n;i++){if(!readmem(o,p+i,s+i,1))return 0;if(!s[i])return 1;}return 0;}
static uint32_t read32(void *o,uint32_t p){(void)o;return rd32(p);}
static struct Soft386GuestMemoryOps m={NULL,valid,readmem,writemem,readstr,read32};
static void owned(const void *p){assert((uintptr_t)p<(uintptr_t)ram||(uintptr_t)p>=(uintptr_t)ram+sizeof(ram));}
static int f_socket(int d,int t,int p){assert(d==2&&t==1&&!p);return socket(AF_INET,SOCK_STREAM,0);}
static int f_connect(int s,const void *p,int n){owned(p);assert(n==16);return connect(s,p,n);}
static int f_send(int s,const void *p,int n,int flags){owned(p);return (int)send(s,p,n,flags);}
static int f_recv(int s,void *p,int n,int flags){int v;owned(p);atomic_store(&recv_entered,1);while(!atomic_load(&recv_leave))usleep(1000);v=(int)recv(s,p,n,flags);if(v<0)err=10054;return v;}
static int f_select(int *s,int r,int w,int e,int32_t t){owned(s);assert(r==1&&w==1&&!e&&t==0);assert(s[0]==s[1]);s[0]=-1;return 1;}
static int f_accept(int s,void *p,int *n){(void)s;owned(p);owned(n);assert(*n==16);memset(p,0x2a,16);*n=16;return 400;}
static int f_close(int s){return close(s);}
static int f_errno(void){return err;}
static void f_set(int e){err=e;}
static int f_herrno(void){return 0;}
static int f_init(void){return 0;}
static Os2NetHost *f_host(const char *n){
 static char address[]={127,0,0,1};static char *aliases[]={"alias-one","alias-two",NULL};static char *addresses[]={address,NULL};
 static Os2NetHost h={"localhost",aliases,2,4,addresses};owned(n);assert(!strcmp(n,"localhost"));return &h;
}
static Os2NetService *f_service(const char *n,const char *p){static char *aliases[]={"terminal",NULL};static Os2NetService s={"telnet",aliases,0x1700,"tcp"};owned(n);owned(p);assert(!strcmp(n,"telnet")&&!strcmp(p,"tcp"));return &s;}
static void *so(void *o,uint32_t n){(void)o;switch(n){
#define P(n,f) case n:return (void *)(uintptr_t)f
 P(1,f_accept);P(12,f_select);P(3,f_connect);P(10,f_recv);P(13,f_send);P(16,f_socket);P(17,f_close);P(20,f_errno);P(26,f_init);P(35,f_set);default:return NULL;}}
static void *tcp(void *o,uint32_t n){(void)o;switch(n){P(11,f_host);P(24,f_service);P(51,f_herrno);default:return NULL;}}
#undef P
static uint32_t dispatch(unsigned mod,unsigned ord,unsigned slot){int handled;uint32_t r=soft386_net_dispatch(&b,&m,mod,ord,0x1000+slot*0x100,slot,slot+1,0x10000+slot*0x30000+(mod==1&&(ord==23||ord==24)?0x10000:0),&handled);assert(handled);calls++;return r;}
static uint32_t waitcall(unsigned mod,unsigned ord,unsigned slot){unsigned n;uint32_t r;for(n=0;n<5000;n++){r=dispatch(mod,ord,slot);if(!b.waiting)return r;usleep(1000);}assert(!"network timeout");return 0;}
static void args(unsigned slot,unsigned n,...){va_list v;unsigned i;va_start(v,n);for(i=0;i<n;i++)w32(0x1004+slot*0x100+i*4,va_arg(v,uint32_t));va_end(v);}
int main(void){
 int listener,peer,client;struct sockaddr_in a; socklen_t len=sizeof(a);uint32_t p,q;
 memset(&b,0,sizeof(b));soft386_doscalls_bridge_set_provider(&b.modules[0],NULL,so,NULL,0);soft386_doscalls_bridge_set_provider(&b.modules[1],NULL,tcp,NULL,0);
 listener=socket(AF_INET,SOCK_STREAM,0);assert(listener>=0);memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);assert(!bind(listener,(void *)&a,sizeof(a)));assert(!getsockname(listener,(void *)&a,&len));assert(!listen(listener,1));
 args(0,3,2u,1u,0u);client=(int)waitcall(0,16,0);assert(client>=0);
 memcpy(ram+0x2000,&a,16);args(0,3,(uint32_t)client,0x2000u,16u);assert(!waitcall(0,3,0));peer=accept(listener,NULL,NULL);assert(peer>=0);
 /* Native receive is held pending. The second guest slot still sends. */
 args(0,4,(uint32_t)client,0x3000u,8u,0u);assert(!dispatch(0,10,0)&&b.waiting);
 while(!atomic_load(&recv_entered))usleep(1000);
 memcpy(ram+0x3100,"jar\0data",8);args(1,4,(uint32_t)client,0x3100u,8u,0u);assert(dispatch(0,13,1)==0&&b.waiting);
 memset(ram+0x3100,'X',8);assert(waitcall(0,13,1)==8);{char buf[8];assert(recv(peer,buf,8,MSG_WAITALL)==8&&!memcmp(buf,"jar\0data",8));assert(send(peer,buf,8,0)==8);}
 memset(ram+0x3000,0xCC,8);assert(!dispatch(0,10,0)&&b.waiting);assert(ram[0x3000]==0xCC);atomic_store(&recv_leave,1);assert(waitcall(0,10,0)==8&&!memcmp(ram+0x3000,"jar\0data",8));
 /* IBM SELECT uses an array of integers, not a native fd_set. */
 w32(0x3600,client);w32(0x3604,client);args(0,5,0x3600u,1u,1u,0u,0u);assert(waitcall(0,12,0)==1&&rd32(0x3600)==UINT32_MAX&&rd32(0x3604)==(uint32_t)client);
 args(0,5,0x3600u,0xffffffffu,1u,0u,0u);assert(dispatch(0,12,0)==UINT32_MAX&&!b.waiting);
 /* Reject malformed ACCEPT outputs before it can consume a connection. */
 w32(0x3650,16);args(0,3,(uint32_t)client,((uint32_t)sizeof(ram)-8u),0x3650u);assert(dispatch(0,1,0)==UINT32_MAX&&!b.waiting);
 /* Resolver graph contains guest pointers, including both NULL-terminated lists. */
 strcpy((char *)ram+0x3200,"localhost");args(0,1,0x3200u);p=waitcall(1,11,0);assert(p>=0x10000&&p<0x20000);assert(!strcmp((char *)ram+rd32(p),"localhost"));assert(rd32(p+8)==2&&rd32(p+12)==4);q=rd32(p+4);assert(!strcmp((char *)ram+rd32(q),"alias-one")&&!strcmp((char *)ram+rd32(q+4),"alias-two")&&!rd32(q+8));q=rd32(p+16);assert(!memcmp(ram+rd32(q),"\x7f\0\0\1",4)&&!rd32(q+4));
 strcpy((char *)ram+0x3300,"telnet");strcpy((char *)ram+0x3400,"tcp");args(1,2,0x3300u,0x3400u);p=waitcall(1,24,1);assert(p>=0x50000&&p<0x60000&&rd32(p+8)==0x1700);assert(!strcmp((char *)ram+rd32(p+12),"tcp"));
 /* A service lookup must not invalidate this thread's previous hostent. */
 args(0,2,0x3300u,0x3400u);p=waitcall(1,24,0);assert(p>=0x20000&&p<0x30000);assert(!strcmp((char *)ram+rd32(0x10004),"localhost"));
 args(0,4,(uint32_t)client,((uint32_t)sizeof(ram)-1u),8u,0u);assert(dispatch(0,13,0)==UINT32_MAX&&!b.waiting);assert(dispatch(0,20,0)==10014);assert(dispatch(0,20,1)==0);
 args(0,1,((uint32_t)sizeof(ram)-1u));ram[sizeof(ram)-1]='x';assert(dispatch(1,11,0)==0&&!b.waiting);assert(dispatch(1,51,0)==3);
 /* Close/cancel fences an older in-flight operation. Its late completion
  * must never be committed into a newer socket lifetime. */
 atomic_store(&recv_entered,0);atomic_store(&recv_leave,0);memset(ram+0x3000,0xA5,8);
 args(0,4,(uint32_t)client,0x3000u,8u,0u);assert(!dispatch(0,10,0)&&b.waiting);
 while(!atomic_load(&recv_entered))usleep(1000);
 args(1,1,(uint32_t)client);assert(!waitcall(0,17,1));
 atomic_store(&recv_leave,1);assert(waitcall(0,10,0)==UINT32_MAX);
 assert(dispatch(0,20,0)==10004);assert(ram[0x3000]==0xA5);
 args(0,4,(uint32_t)client,0x3000u,8u,0u);assert(dispatch(0,13,0)==UINT32_MAX&&dispatch(0,20,0)==10038);
 soft386_net_close(&b);assert(!b.pending_on_close);close(peer);close(listener);
 printf("R5C network PASS: loopback binary I/O, pending read with concurrent sender, copied inputs, guest resolver graphs, thread errno, bounds, close generation fence, stale sockets, async beep (%d dispatches)\n",calls);return 0;
}
