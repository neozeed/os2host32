/* QUECALLS remains a native service. Only metadata crosses the boundary:
 * its existing implementation stores pData as an opaque 32-bit VALUE and
 * never follows it. In particular Sarien sends beep frequencies this way.
 * A WAIT read always calls native NOWAIT; the vessel parks/retries the guest
 * thread so a writer in the same jar can actually run. */
#include "soft386_queue_bridge.h"
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifndef __cdecl
#define __cdecl
#endif
#define QUE_INVALID_PARAMETER 87u
#define QUE_INVALID_HANDLE 337u
#define QUE_EMPTY 342u
#define QUE_TOKEN_BASE 0x6e000001u
uint32_t soft386_queue_named_ordinal(const char *name)
{
    if(!strcmp(name,"DosReadQueue"))return 9;
    if(!strcmp(name,"DosWriteQueue"))return 14;
    if(!strcmp(name,"DosOpenQueue"))return 15;
    if(!strcmp(name,"DosCreateQueue"))return 16;
    return 0;
}
#ifdef _WIN32
static void *queue_proc(void *h,uint32_t ordinal){return (void *)(uintptr_t)GetProcAddress((HMODULE)h,(LPCSTR)(uintptr_t)ordinal);}
static void queue_close(void *h){FreeLibrary((HMODULE)h);}
#endif
static int open_queue(struct Soft386QueueBridge *b)
{
    if(b->module.loaded)return 1;
    if(b->disabled)return 0;
#ifdef _WIN32
    b->module.opaque=LoadLibraryA(b->path&&*b->path?b->path:"QUECALLS.dll");
    if(b->module.opaque){b->module.loaded=1;b->module.get_proc=queue_proc;b->module.close=queue_close;return 1;}
#endif
    if(b->trace)fprintf(stderr,"soft386: native QUECALLS.dll unavailable\n");
    return 0;
}
int soft386_queue_export(struct Soft386QueueBridge *b,uint32_t ordinal)
{
    if(!open_queue(b)||!b->module.get_proc)return 0;
    return b->module.get_proc(b->module.opaque,ordinal)!=NULL;
}
static uint32_t queue_token(struct Soft386QueueBridge *b,uint32_t native)
{
    unsigned i;
    if(!native)return 0;
    for(i=0;i<S386_QUEUE_HANDLES;i++)if(b->native[i]==native)return QUE_TOKEN_BASE+i;
    for(i=0;i<S386_QUEUE_HANDLES;i++)if(!b->native[i]){b->native[i]=native;return QUE_TOKEN_BASE+i;}
    return 0;
}
static int output(const struct Soft386GuestMemoryOps *m,uint32_t p,uint32_t n){return p&&m->valid(m->opaque,p,n,1);}
uint32_t soft386_queue_dispatch(struct Soft386QueueBridge *b,const struct Soft386GuestMemoryOps *m,uint32_t ordinal,uint32_t esp,int *handled)
{
    uint32_t a[8],count=ordinal==9?8:(ordinal==14?5:3),i,rc,native=0,token,owner=0,length=0,request[2]={0};
    uintptr_t data=0;unsigned char priority=0;char name[260];void *fn;
    int known=ordinal==9||ordinal==14||ordinal==15||ordinal==16;
    if(handled)*handled=0;b->waiting=0;
    if(!soft386_queue_export(b,ordinal)){
        if(b->trace)fprintf(stderr,"soft386: service QUECALLS.%u native export missing\n",ordinal);
        if(handled)*handled=1;
        return 1;
    }
    if(!known){
        if(b->trace)fprintf(stderr,"soft386: service QUECALLS.%u export resolved; ABI descriptor missing; call not attempted\n",ordinal);
        if(handled)*handled=1;
        return 1;
    }
    if(handled)*handled=1;
    if(!esp||esp>UINT32_MAX-4-count*4||!m->valid(m->opaque,esp+4,count*4,0))return QUE_INVALID_PARAMETER;
    for(i=0;i<count;i++)a[i]=m->read_u32(m->opaque,esp+4+4*i);
    /* Validate every output before a read can consume an entry. */
    if(ordinal==16){if(!output(m,a[0],4)||!a[2]||!m->read_cstr(m->opaque,a[2],name,sizeof(name)))return QUE_INVALID_PARAMETER;}
    else if(ordinal==15){if(!output(m,a[0],4)||!output(m,a[1],4)||!a[2]||!m->read_cstr(m->opaque,a[2],name,sizeof(name)))return QUE_INVALID_PARAMETER;}
    else{
        if(a[0]<QUE_TOKEN_BASE||a[0]-QUE_TOKEN_BASE>=S386_QUEUE_HANDLES||!(native=b->native[a[0]-QUE_TOKEN_BASE]))return QUE_INVALID_HANDLE;
        if(ordinal==9){
            if(!output(m,a[1],8)||!output(m,a[2],4)||!output(m,a[3],4)||!output(m,a[6],1)||a[7])return QUE_INVALID_PARAMETER;
            if(a[5]>1)return 433;
        }
    }
    fn=b->module.get_proc(b->module.opaque,ordinal);
    if(b->trace)fprintf(stderr,"soft386: native QUECALLS.%u [marshalled; scheduler owns waits]\n",ordinal);
    if(ordinal==16)rc=((uint32_t (__cdecl *)(uint32_t *,uint32_t,const char *))fn)(&native,a[1],name);
    else if(ordinal==15)rc=((uint32_t (__cdecl *)(uint32_t *,uint32_t *,const char *))fn)(&owner,&native,name);
    else if(ordinal==14)return ((uint32_t (__cdecl *)(uint32_t,uint32_t,uint32_t,uintptr_t,uint32_t))fn)(native,a[1],a[2],(uintptr_t)a[3],a[4]);
    else{
        rc=((uint32_t (__cdecl *)(uint32_t,uint32_t *,uint32_t *,uintptr_t *,uint32_t,uint32_t,unsigned char *,uint32_t))fn)(native,request,&length,&data,a[4],1,&priority,0);
        if(rc==QUE_EMPTY&&!a[5]){b->waiting=1;return 0;}
        if(rc)return rc;
        if(data>UINT32_MAX)return QUE_INVALID_PARAMETER;
        /* This backend's queues are process-local. Use the same jar PID as
         * DosGetInfoBlocks, never its host PID or a host pointer. */
        request[0]=b->pid?b->pid:1;token=(uint32_t)data;
        if(!m->write(m->opaque,a[1],request,8)||!m->write(m->opaque,a[2],&length,4)||!m->write(m->opaque,a[3],&token,4)||!m->write(m->opaque,a[6],&priority,1))return QUE_INVALID_PARAMETER;
        return 0;
    }
    if(rc)return rc;
    token=queue_token(b,native);if(!token)return 334;
    owner=b->pid?b->pid:1;
    if(ordinal==15&&!m->write(m->opaque,a[0],&owner,4))return QUE_INVALID_PARAMETER;
    if(!m->write(m->opaque,ordinal==15?a[1]:a[0],&token,4))return QUE_INVALID_PARAMETER;
    return 0;
}
void soft386_queue_close(struct Soft386QueueBridge *b)
{
    if(b->module.close&&b->module.opaque)b->module.close(b->module.opaque);
    memset(&b->module,0,sizeof(b->module));memset(b->native,0,sizeof(b->native));b->waiting=0;
}
