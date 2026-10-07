/* R5C: copied requests, asynchronous native I/O, guest-owned result graphs.
 * Worker threads NEVER read or write the emulator's RAM or CPU. A completed
 * request is committed on its owner guest thread when its veneer retries.
 * Native TLS errno and resolver output are captured before the worker exits. */
#include "soft386_net_bridge.h"
#include "os2_net.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <stdatomic.h>
#endif
#ifndef __cdecl
#define __cdecl
#endif
#define LIMIT 0x01000000u
struct Blob { uint32_t guest,capacity,written; unsigned char *data; int output; };
struct Soft386NetJob {
    unsigned module,ordinal; uint32_t esp,a[6],arena,rc;
    int error,herror;
    int tracked_socket; uint32_t socket_generation;
    struct Blob blob[3];
    unsigned char result[S386_NET_ARENA]; uint32_t used;
    void *fn,*seterr,*geterr,*getherr;
#ifdef _WIN32
    HANDLE thread;
#else
    pthread_t thread; atomic_int done;
#endif
};
static const char *so_names[36]={[1]="ACCEPT",[2]="BIND",[3]="CONNECT",[5]="GETPEERNAME",[6]="GETSOCKNAME",[7]="GETSOCKOPT",[8]="IOCTL",[9]="LISTEN",[10]="RECV",[11]="RECVFROM",[12]="SELECT",[13]="SEND",[14]="SENDTO",[15]="SETSOCKOPT",[16]="SOCKET",[17]="SOCLOSE",[20]="SOCK_ERRNO",[25]="SHUTDOWN",[26]="SOCK_INIT",[35]="SET_ERRNO"};
static const char *tcp_names[52]={[3]="LSWAP",[4]="BSWAP",[5]="INET_ADDR",[10]="INET_NTOA",[11]="GETHOSTBYNAME",[12]="GETHOSTBYADDR",[23]="GETSERVBYPORT",[24]="GETSERVBYNAME",[44]="GETHOSTNAME",[51]="TCP_H_ERRNO"};
uint32_t soft386_net_named_ordinal(unsigned m,const char *name)
{
    unsigned i,n=m?52:36; const char **names=m?tcp_names:so_names;
    for(i=0;i<n;i++)if(names[i]&&!strcmp(names[i],name))return i;return 0;
}
static unsigned nargs(unsigned m,unsigned o)
{
    if(m==1){switch(o){case 3:case 4:case 5:case 10:case 11:return 1;case 12:return 3;case 23:case 24:case 44:return 2;case 51:return 0;default:return 99;}}
    switch(o){case 1:case 2:case 3:case 5:case 6:case 16:return 3;case 7:case 12:case 15:return 5;case 8:case 10:case 13:return 4;case 11:case 14:return 6;case 9:case 25:return 2;case 17:case 35:return 1;case 20:case 26:return 0;default:return 99;}
}
#ifdef _WIN32
static void *proc(void *h,uint32_t o){return (void *)(uintptr_t)GetProcAddress((HMODULE)h,(LPCSTR)(uintptr_t)o);}
static void unload(void *h){FreeLibrary((HMODULE)h);}
#endif
static int open_modules(struct Soft386NetBridge *b,unsigned m)
{
    unsigned i;if(b->disabled)return 0;
    for(i=0;i<=m;i++)if(!b->modules[i].loaded){
#ifdef _WIN32
        HMODULE h=LoadLibraryA(b->paths[i]&&*b->paths[i]?b->paths[i]:(i?"TCP32DLL.dll":"SO32DLL.dll"));
        if(h){soft386_doscalls_bridge_set_provider(&b->modules[i],h,proc,unload,b->trace);continue;}
        fprintf(stderr,"soft386: cannot load native %s (Win32 error %lu)\n",i?"TCP32DLL":"SO32DLL",(unsigned long)GetLastError());
#endif
        return 0;
    }return 1;
}
static void *getfn(struct Soft386NativeDoscalls *b,unsigned o){return b->loaded&&b->get_proc?b->get_proc(b->opaque,o):NULL;}
static int valid(const struct Soft386GuestMemoryOps *m,uint32_t p,uint32_t n,int w){return n<=LIMIT&&(!n||(p&&m->valid(m->opaque,p,n,w)));}
static int blob(struct Soft386NetJob *j,unsigned i,const struct Soft386GuestMemoryOps *m,uint32_t p,uint32_t n,int in,int out)
{
    struct Blob *b=&j->blob[i];
    if(!valid(m,p,n,out)|| (in&&!valid(m,p,n,0)))return 0;
    b->guest=p;b->capacity=n;b->output=out;b->data=calloc(1,n?n:1);
    return b->data&&(!in||!n||m->read(m->opaque,p,b->data,n));
}
static int string(struct Soft386NetJob *j,unsigned i,const struct Soft386GuestMemoryOps *m,uint32_t p,int optional)
{
    if(!p&&optional)return 1;
    j->blob[i].data=calloc(1,65536);
    return p&&j->blob[i].data&&m->read_cstr(m->opaque,p,(char *)j->blob[i].data,65536);
}
static int outlength(struct Soft386NetJob *j,unsigned i,const struct Soft386GuestMemoryOps *m,uint32_t buf,uint32_t len,int optional)
{
    uint32_t n;
    if(!buf&&!len&&optional)return 1;
    if(!len||!valid(m,len,4,0)||!valid(m,len,4,1))return 0;
    n=m->read_u32(m->opaque,len);
    return blob(j,i,m,buf,n,0,1)&&blob(j,i+1,m,len,4,1,1);
}
static void release(struct Soft386NetJob *j){unsigned i;for(i=0;i<3;i++)free(j->blob[i].data);free(j);}
static void put32(unsigned char *p,uint32_t v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
static uint32_t take(struct Soft386NetJob *j,uint32_t n)
{
    uint32_t p=(j->used+3u)&~3u;if(n>S386_NET_ARENA-p)return 0;j->used=p+n;return p;
}
static uint32_t copy_string(struct Soft386NetJob *j,const char *s)
{
    size_t n;uint32_t p;if(!s)return 0;
    for(n=0;n<S386_NET_ARENA&&s[n];n++);if(n==S386_NET_ARENA)return 0;
    p=take(j,(uint32_t)n+1);if(p)memcpy(j->result+p,s,n+1);return p;
}
static uint32_t copy_list(struct Soft386NetJob *j,char *const *list,unsigned item)
{
    unsigned n=0,i;uint32_t p,q;
    if(list)while(list[n])if(++n>4096)return 0;
    p=take(j,(n+1u)*4u);if(!p)return 0;
    for(i=0;i<n;i++){
        q=item?take(j,item):copy_string(j,list[i]);if(!q)return 0;
        if(item)memcpy(j->result+q,list[i],item);
        put32(j->result+p+4*i,j->arena+q);
    }return p;
}
static uint32_t host_result(struct Soft386NetJob *j,const Os2NetHost *h)
{
    uint32_t p,n,a,v;if(!h)return 0;
    if(h->family!=2||h->length!=4)goto bad;
    p=take(j,20);n=copy_string(j,h->name);a=copy_list(j,h->aliases,0);v=copy_list(j,h->addresses,4);
    if(!p||!n||!a||!v)goto bad;
    put32(j->result+p,j->arena+n);put32(j->result+p+4,j->arena+a);put32(j->result+p+8,2);put32(j->result+p+12,4);put32(j->result+p+16,j->arena+v);return j->arena+p;
 bad:j->error=10055;j->herror=3;j->used=0;return 0;
}
static uint32_t service_result(struct Soft386NetJob *j,const Os2NetService *s)
{
    uint32_t p,n,a,v;if(!s)return 0;p=take(j,16);n=copy_string(j,s->name);a=copy_list(j,s->aliases,0);v=copy_string(j,s->protocol);
    if(!p||!n||!a||!v){j->error=10055;j->used=0;return 0;}
    put32(j->result+p,j->arena+n);put32(j->result+p+4,j->arena+a);put32(j->result+p+8,(uint32_t)s->port);put32(j->result+p+12,j->arena+v);return j->arena+p;
}
#define FN(t) ((t)j->fn)
static void execute(struct Soft386NetJob *j)
{
    uint32_t *a=j->a;void *p=j->blob[0].data,*q=j->blob[1].data,*r=j->blob[2].data;
    const Os2NetHost *h=NULL;const Os2NetService *s=NULL;const char *str=NULL;
    unsigned i;int n;
    if(j->seterr)((void (__cdecl *)(int))j->seterr)(j->error);
    if(!j->module)switch(j->ordinal){
    case 1:case 5:case 6:j->rc=FN(int (__cdecl *)(int,void *,int *))(a[0],p,q);break;
    case 2:case 3:j->rc=FN(int (__cdecl *)(int,const void *,int))(a[0],p,a[2]);break;
    case 7:j->rc=FN(int (__cdecl *)(int,int,int,void *,int *))(a[0],a[1],a[2],p,q);break;
    case 8:j->rc=FN(int (__cdecl *)(int,uint32_t,void *,int))(a[0],a[1],p,a[3]);break;
    case 9:case 25:j->rc=FN(int (__cdecl *)(int,int))(a[0],a[1]);break;
    case 10:case 13:j->rc=FN(int (__cdecl *)(int,void *,int,int))(a[0],p,a[2],a[3]);break;
    case 11:j->rc=FN(int (__cdecl *)(int,void *,int,int,void *,int *))(a[0],p,a[2],a[3],q,r);break;
    case 12:j->rc=FN(int (__cdecl *)(int *,int,int,int,int32_t))(p,a[1],a[2],a[3],(int32_t)a[4]);break;
    case 14:j->rc=FN(int (__cdecl *)(int,const void *,int,int,const void *,int))(a[0],p,a[2],a[3],q,a[5]);break;
    case 15:j->rc=FN(int (__cdecl *)(int,int,int,const void *,int))(a[0],a[1],a[2],p,a[4]);break;
    case 16:j->rc=FN(int (__cdecl *)(int,int,int))(a[0],a[1],a[2]);break;
    case 17:j->rc=FN(int (__cdecl *)(int))(a[0]);break;
    case 26:j->rc=FN(int (__cdecl *)(void))();break;
    }
    else switch(j->ordinal){
    case 3:j->rc=FN(uint32_t (__cdecl *)(uint32_t))(a[0]);break;
    case 4:j->rc=FN(uint16_t (__cdecl *)(uint16_t))((uint16_t)a[0]);break;
    case 5:j->rc=FN(uint32_t (__cdecl *)(const char *))(p);break;
    case 10:str=FN(char *(__cdecl *)(uint32_t))(a[0]);break;
    case 11:h=FN(Os2NetHost *(__cdecl *)(const char *))(p);break;
    case 12:h=FN(Os2NetHost *(__cdecl *)(const char *,int,int))(p,a[1],a[2]);break;
    case 23:s=FN(Os2NetService *(__cdecl *)(int,const char *))(a[0],p);break;
    case 24:s=FN(Os2NetService *(__cdecl *)(const char *,const char *))(p,q);break;
    case 44:j->rc=FN(int (__cdecl *)(char *,int))(p,a[1]);break;
    }
    if(j->geterr)j->error=((int (__cdecl *)(void))j->geterr)();
    if(j->module==1&&j->getherr&&(j->ordinal==11||j->ordinal==12))j->herror=((int (__cdecl *)(void))j->getherr)();
    if(j->module==1){
        if(j->ordinal==11||j->ordinal==12)j->rc=host_result(j,h);
        if(j->ordinal==23||j->ordinal==24)j->rc=service_result(j,s);
        if(j->ordinal==10){uint32_t off=copy_string(j,str);j->rc=off?j->arena+off:0;if(str&&!off)j->error=10055;}
    }
    if((int32_t)j->rc<0)return;
    for(i=0;i<3;i++)if(j->blob[i].output)j->blob[i].written=j->blob[i].capacity;
    if(!j->module){
        if(j->ordinal==10||j->ordinal==11)j->blob[0].written=j->rc;
        if(j->ordinal==1||j->ordinal==5||j->ordinal==6||j->ordinal==7){
            if(q){memcpy(&n,q,4);j->blob[0].written=n<0?0:(uint32_t)n;}
        }
        if(j->ordinal==11&&r){memcpy(&n,r,4);j->blob[1].written=n<0?0:(uint32_t)n;}
    }
    for(i=0;i<3;i++)if(j->blob[i].written>j->blob[i].capacity)j->blob[i].written=j->blob[i].capacity;
}
#ifdef _WIN32
static DWORD WINAPI worker(void *p){execute(p);return 0;}
static int start(struct Soft386NetJob *j){j->thread=CreateThread(NULL,0,worker,j,0,NULL);return j->thread!=NULL;}
static int done(struct Soft386NetJob *j){return WaitForSingleObject(j->thread,0)==WAIT_OBJECT_0;}
static void join(struct Soft386NetJob *j){CloseHandle(j->thread);}
#else
static void *worker(void *p){struct Soft386NetJob *j=p;execute(j);atomic_store_explicit(&j->done,1,memory_order_release);return NULL;}
static int start(struct Soft386NetJob *j){atomic_init(&j->done,0);return !pthread_create(&j->thread,NULL,worker,j);}
static int done(struct Soft386NetJob *j){return atomic_load_explicit(&j->done,memory_order_acquire);}
static void join(struct Soft386NetJob *j){pthread_join(j->thread,NULL);}
#endif
static int socket_slot(struct Soft386NetBridge *b,int s)
{
    unsigned i;for(i=0;i<256;i++)if(b->socket_used[i]&&b->sockets[i]==s)return (int)i;return -1;
}
static int owns(struct Soft386NetBridge *b,int s){return socket_slot(b,s)>=0;}
static int socket_generation(struct Soft386NetBridge *b,int s,uint32_t *generation)
{
    int i=socket_slot(b,s);if(i<0)return 0;if(generation)*generation=b->socket_generation[i];return 1;
}
static void remember(struct Soft386NetBridge *b,int s)
{
    unsigned i,free_slot=256;
    for(i=0;i<256;i++){
        if(!b->socket_used[i]&&b->sockets[i]==s){free_slot=i;break;}
        if(free_slot==256&&!b->socket_used[i])free_slot=i;
    }
    if(free_slot<256){
        b->socket_used[free_slot]=1;b->sockets[free_slot]=s;
        if(++b->socket_generation[free_slot]==0)b->socket_generation[free_slot]=1;
    }
}
static uint32_t invalidate_socket(struct Soft386NetBridge *b,int s)
{
    int i=socket_slot(b,s);if(i<0)return 0;
    if(++b->socket_generation[i]==0)b->socket_generation[i]=1;
    return b->socket_generation[i];
}
static void forget(struct Soft386NetBridge *b,int s)
{
    int i=socket_slot(b,s);if(i>=0){
        if(++b->socket_generation[i]==0)b->socket_generation[i]=1;
        b->socket_used[i]=0;
    }
}
static int primary_socket_ordinal(unsigned module,unsigned ordinal)
{
    if(module)return 0;
    switch(ordinal){
    case 1:case 2:case 3:case 5:case 6:case 7:case 8:case 9:
    case 10:case 11:case 13:case 14:case 15:case 17:case 25:return 1;
    default:return 0;
    }
}
static int prepare(struct Soft386NetJob *j,const struct Soft386GuestMemoryOps *m)
{
    uint32_t *a=j->a;uint64_t total;
    if(j->module==1)switch(j->ordinal){
    case 5:case 11:return string(j,0,m,a[0],0);
    case 12:return a[1]==4&&a[2]==2&&blob(j,0,m,a[0],a[1],1,0);
    case 23:return string(j,0,m,a[1],1);
    case 24:return string(j,0,m,a[0],0)&&string(j,1,m,a[1],1);
    case 44:return blob(j,0,m,a[0],a[1],0,1);
    default:return 1;
    }
    switch(j->ordinal){
    case 1:case 5:case 6:return outlength(j,0,m,a[1],a[2],j->ordinal==1);
    case 2:case 3:return blob(j,0,m,a[1],a[2],1,0);
    case 7:return outlength(j,0,m,a[3],a[4],0);
    case 8:return blob(j,0,m,a[2],a[3],1,1);
    case 10:return blob(j,0,m,a[1],a[2],0,1);
    case 11:return blob(j,0,m,a[1],a[2],0,1)&&outlength(j,1,m,a[4],a[5],1);
    case 12:total=(uint64_t)a[1]+a[2]+a[3];return total<=768&&blob(j,0,m,a[0],(uint32_t)total*4,1,1);
    case 13:return blob(j,0,m,a[1],a[2],1,0);
    case 14:return blob(j,0,m,a[1],a[2],1,0)&&blob(j,1,m,a[4],a[5],1,0);
    case 15:return blob(j,0,m,a[3],a[4],1,0);
    default:return 1;
    }
}
uint32_t soft386_net_dispatch(struct Soft386NetBridge *b,const struct Soft386GuestMemoryOps *m,unsigned module,uint32_t ordinal,uint32_t esp,unsigned slot,uint32_t tid,uint32_t arena,int *handled)
{
    unsigned i,count=nargs(module,ordinal);struct Soft386NetJob *j;uint32_t rc;int err=10014;
    *handled=count!=99;b->waiting=0;
    if(count==99||slot>=S386_NET_THREADS)return UINT32_MAX;
    if((j=b->jobs[slot])!=NULL){
        if(j->module!=module||j->ordinal!=ordinal||j->esp!=esp||b->tids[slot]!=tid){b->errors[slot]=10022;return UINT32_MAX;}
        if(!done(j)){b->waiting=1;return 0;}
        join(j);rc=j->rc;
        if(j->tracked_socket&&j->ordinal!=17){
            uint32_t generation=0;
            if(!socket_generation(b,j->tracked_socket,&generation)||generation!=j->socket_generation){
                if(b->trace)fprintf(stderr,"soft386: net stale completion %s.%u %s TID=%u socket=%d job_gen=%u current_gen=%u rc=%d discarded\n",
                    j->module?"TCP32DLL":"SO32DLL",j->ordinal,j->module?tcp_names[j->ordinal]:so_names[j->ordinal],tid,j->tracked_socket,
                    (unsigned)j->socket_generation,(unsigned)generation,(int32_t)rc);
                b->errors[slot]=10004;release(j);b->jobs[slot]=NULL;return UINT32_MAX;
            }
        }
        if(b->trace)fprintf(stderr,"soft386: net complete %s.%u %s TID=%u socket=%d gen=%u rc=%d errno=%d\n",
            j->module?"TCP32DLL":"SO32DLL",j->ordinal,j->module?tcp_names[j->ordinal]:so_names[j->ordinal],tid,j->tracked_socket,
            (unsigned)j->socket_generation,(int32_t)rc,j->error);
        /* Revalidate all destinations: another guest thread may have freed them. */
        for(i=0;i<3;i++)if(j->blob[i].written&&!valid(m,j->blob[i].guest,j->blob[i].written,1))goto bad_commit;
        if(j->used>4&&!valid(m,arena,j->used,1))goto bad_commit;
        for(i=0;i<3;i++)if(j->blob[i].written&&!m->write(m->opaque,j->blob[i].guest,j->blob[i].data,j->blob[i].written))goto bad_commit;
        if(j->used>4&&!m->write(m->opaque,arena,j->result,j->used))goto bad_commit;
        if(!module&&(int32_t)rc>=0){if(ordinal==1||ordinal==16)remember(b,(int)rc);if(ordinal==17)forget(b,(int)j->a[0]);}
        b->errors[slot]=j->error;b->herrors[slot]=j->herror;release(j);b->jobs[slot]=NULL;return rc;
 bad_commit:
        /* Preserve ownership of a successfully accepted descriptor even when
         * the caller's output disappeared while the native call was pending. */
        if(!module&&ordinal==1&&(int32_t)rc>=0){void *closefn=getfn(&b->modules[0],17);if(closefn)((int (__cdecl *)(int))closefn)((int)rc);}
        b->errors[slot]=10014;release(j);b->jobs[slot]=NULL;return UINT32_MAX;
    }
    if(b->tids[slot]!=tid){b->tids[slot]=tid;b->errors[slot]=b->herrors[slot]=0;}
    if(!module&&ordinal==20)return b->errors[slot];
    if(module==1&&ordinal==51)return b->herrors[slot];
    if(esp>UINT32_MAX-4-count*4||!valid(m,esp+4,count*4,0))goto error;
    if(!module&&ordinal==35){b->errors[slot]=(int)m->read_u32(m->opaque,esp+4);return 0;}
    j=calloc(1,sizeof(*j));if(!j){err=10055;goto error;}
    j->module=module;j->ordinal=ordinal;j->esp=esp;j->arena=arena;j->used=4;j->rc=UINT32_MAX;
    j->error=b->errors[slot];j->herror=b->herrors[slot];
    for(i=0;i<count;i++)j->a[i]=m->read_u32(m->opaque,esp+4+4*i);
    if(!prepare(j,m)){release(j);goto error;}
    if(!module){
        if(ordinal!=12&&ordinal!=16&&ordinal!=26&&!owns(b,(int)j->a[0])){release(j);err=10038;goto error;}
        if(ordinal==12){uint32_t n=j->blob[0].capacity/4;for(i=0;i<n;i++){int s;memcpy(&s,j->blob[0].data+i*4,4);if(s!=-1&&!owns(b,s)){release(j);err=10038;goto error;}}}
        if(primary_socket_ordinal(module,ordinal)){
            j->tracked_socket=(int)j->a[0];
            if(!socket_generation(b,j->tracked_socket,&j->socket_generation)){release(j);err=10038;goto error;}
        }
    }
    if(open_modules(b,module)){
        j->fn=getfn(&b->modules[module],ordinal);j->geterr=getfn(&b->modules[0],20);j->seterr=getfn(&b->modules[0],35);j->getherr=module?getfn(&b->modules[1],51):NULL;
    }
    if(!j->fn){release(j);err=10045;goto error;}
    if(!start(j)){release(j);err=10055;goto error;}
    if(!module&&ordinal==17){
        uint32_t generation=invalidate_socket(b,j->tracked_socket);
        if(b->trace)fprintf(stderr,"soft386: net cancel fence SO32DLL.17 SOCLOSE TID=%u socket=%d new_gen=%u\n",tid,j->tracked_socket,(unsigned)generation);
    }
    b->jobs[slot]=j;b->waiting=1;
    if(b->trace)fprintf(stderr,"soft386: net start %s.%u %s TID=%u socket=%d gen=%u\n",
        module?"TCP32DLL":"SO32DLL",ordinal,module?tcp_names[ordinal]:so_names[ordinal],tid,j->tracked_socket,(unsigned)j->socket_generation);
    return 0;
 error:b->errors[slot]=err;
    if(module==1&&(ordinal==10||ordinal==11||ordinal==12||ordinal==23||ordinal==24)){if(ordinal==11||ordinal==12)b->herrors[slot]=3;return 0;}
    return UINT32_MAX;
}
void soft386_net_quiesce_process(struct Soft386NetBridge *b)
{
    unsigned i;int pending=0;void *fn;
    if(!b)return;
    fn=getfn(&b->modules[0],17);
    for(i=0;i<256;i++)if(b->socket_used[i]){
        /* Fence first so an in-flight completion cannot become current again
         * while the vessel is on its way out. */
        if(++b->socket_generation[i]==0)b->socket_generation[i]=1;
        if(fn)((int (__cdecl *)(int))fn)(b->sockets[i]);
        b->socket_used[i]=0;
    }
    for(i=0;i<S386_NET_THREADS;i++)if(b->jobs[i]){
        if(done(b->jobs[i])){join(b->jobs[i]);release(b->jobs[i]);b->jobs[i]=NULL;}
        else pending=1;
    }
    /* Never unload providers from the final Win32 vessel path.  A pending
     * resolver/socket worker may still be executing provider code, and even
     * with no pending worker process-owned DLL mappings are cheaper and safer
     * to reclaim atomically at process termination. */
    b->pending_on_close=pending;
    b->waiting=0;
}

void soft386_net_close(struct Soft386NetBridge *b)
{
    unsigned i;
    soft386_net_quiesce_process(b);
    if(!b->pending_on_close)for(i=0;i<2;i++)soft386_doscalls_bridge_close(&b->modules[i]);
}
