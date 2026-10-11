/* Shared test provider also backs the real-CPU fixture, where an empty queue
 * reader must yield to its writer. */
#include "soft386_queue_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char qram[4096];
static unsigned qcalls,qcount;static uint32_t qvalue,qrequest,qlength;static unsigned char qpriority;
static int qvalid(void *o,uint32_t p,uint32_t n,int w){(void)o;(void)w;return p&&p<sizeof(qram)&&n<=sizeof(qram)-p;}
static int qread(void *o,uint32_t p,void *v,uint32_t n){if(!qvalid(o,p,n,0))return 0;memcpy(v,qram+p,n);return 1;}
static int qwrite(void *o,uint32_t p,const void *v,uint32_t n){if(!qvalid(o,p,n,1))return 0;memcpy(qram+p,v,n);return 1;}
static int qstr(void *o,uint32_t p,char *v,uint32_t n){uint32_t i;for(i=0;i<n;i++){if(!qread(o,p+i,v+i,1))return 0;if(!v[i])return 1;}return 0;}
static uint32_t qget(void *o,uint32_t p){uint32_t v=0;qread(o,p,&v,4);return v;}
static void qput(uint32_t p,uint32_t v){assert(qwrite(NULL,p,&v,4));}
static void qhost(const void *p){assert(p&&(uintptr_t)p>sizeof(qram));assert((uintptr_t)p<(uintptr_t)qram||(uintptr_t)p>=(uintptr_t)qram+sizeof(qram));}
static uint32_t qcreate(uint32_t *h,uint32_t flags,const char *name){qhost(h);qhost(name);assert(flags==0&&!strcmp(name,"\\QUEUES\\jar"));*h=7;qcalls++;return 0;}
static uint32_t qopen(uint32_t *pid,uint32_t *h,const char *name){qhost(pid);qhost(h);qhost(name);assert(!strcmp(name,"\\QUEUES\\jar"));*pid=0xe1234567;*h=7;qcalls++;return 0;}
static uint32_t qsend(uint32_t h,uint32_t req,uint32_t len,uintptr_t value,uint32_t pri){assert(h==7&&!qcount&&value<=UINT32_MAX&&pri<=15);qrequest=req;qlength=len;qvalue=(uint32_t)value;qpriority=(unsigned char)pri;qcount=1;qcalls++;return 0;}
static uint32_t qreceive(uint32_t h,uint32_t *req,uint32_t *len,uintptr_t *value,uint32_t element,uint32_t wait,unsigned char *pri,uint32_t hev){assert(h==7&&!element&&wait==1&&!hev);qhost(req);qhost(len);qhost(value);qhost(pri);qcalls++;if(!qcount)return 342;req[0]=0xe1234567;req[1]=qrequest;*len=qlength;*value=qvalue;*pri=qpriority;qcount=0;return 0;}
static uint32_t qpurge(uint32_t h){assert(h==7);qcount=0;qcalls++;return 0;}
static uint32_t qclosehandle(uint32_t h){assert(h==7);qcalls++;return 0;}
static uint32_t qquery(uint32_t h,uint32_t *count){assert(h==7);qhost(count);*count=qcount;qcalls++;return 0;}
static void *qprovider(void *o,uint32_t ord){(void)o;switch(ord){case 10:return (void *)qpurge;case 11:return (void *)qclosehandle;case 12:return (void *)qquery;case 16:return (void *)qcreate;case 15:return (void *)qopen;case 14:return (void *)qsend;case 9:return (void *)qreceive;case 99:return (void *)qsend;default:return NULL;}}
#ifndef QUEUE_PROVIDER_ONLY
static const struct Soft386GuestMemoryOps qm={NULL,qvalid,qread,qwrite,qstr,qget};
static uint32_t qrun(struct Soft386QueueBridge *b,uint32_t ord,const uint32_t *a,unsigned n){unsigned i;int handled=0;uint32_t rc;for(i=0;i<n;i++)qput(260+4*i,a[i]);rc=soft386_queue_dispatch(b,&qm,ord,256,&handled);assert(handled);return rc;}
#define Q(o,...) qrun(&b,o,(uint32_t[]){__VA_ARGS__},sizeof((uint32_t[]){__VA_ARGS__})/4)
int main(void){struct Soft386QueueBridge b={0};uint32_t h,n;b.module.loaded=1;b.module.get_proc=qprovider;strcpy((char *)qram+512,"\\QUEUES\\jar");
    assert(!Q(16,600,0,512));h=qget(NULL,600);assert(h&&h!=7);
    assert(!Q(15,604,608,512)&&qget(NULL,604)==1&&qget(NULL,608)==h);
    memset(qram+700,0xcc,32);assert(Q(9,h,700,708,712,0,1,716,0)==342&&!b.waiting&&qram[700]==0xcc);
    assert(!Q(9,h,700,708,712,0,0,716,0)&&b.waiting&&qram[700]==0xcc);
    n=qcalls;assert(Q(9,h,700,708,712,0,0,716,0x1234)==87&&qcalls==n&&!b.waiting);
    assert(Q(9,h,700,708,712,0,2,716,0)==433&&qcalls==n);
    assert(Q(14,7,123,4,0xffff1234,3)==337&&qcalls==n);
    assert(!Q(14,h,123,4,0xffff1234,3));assert(qcount==1);
    n=qcalls;assert(Q(9,h,700,708,sizeof(qram)-3,0,1,716,0)==87&&qcalls==n&&qcount==1);
    assert(!Q(9,h,700,708,712,0,0,716,0)&&!b.waiting);
    assert(qget(NULL,700)==1&&qget(NULL,704)==123&&qget(NULL,708)==4&&qget(NULL,712)==0xffff1234&&qram[716]==3);
    assert(soft386_queue_named_ordinal("DosPurgeQueue")==10);
    assert(soft386_queue_named_ordinal("DosCloseQueue")==11);
    assert(soft386_queue_named_ordinal("DosQueryQueue")==12);
    assert(!Q(14,h,123,4,0xffffffff,3));
    qram[724]=0xa5;assert(!Q(12,h,720)&&qget(NULL,720)==1&&qram[724]==0xa5);
    n=qcalls;assert(Q(12,h,4094)==87&&qcalls==n);
    assert(!Q(10,h));assert(!Q(12,h,720)&&qget(NULL,720)==0);
    assert(!Q(11,h));n=qcalls;assert(Q(12,h,720)==337&&qcalls==n);
    assert(!Q(16,600,0,512));assert(qget(NULL,600)!=h);
    n=qcalls;assert(Q(11,h)==337&&qcalls==n);
    soft386_queue_close(&b);b.module.loaded=1;b.module.get_proc=qprovider;assert(Q(14,h,0,0,0,0)==337);
    puts("Soft386 R5B queue boundary PASS: typed handles, opaque payloads, checked copyout, nonblocking waits, jar PID");return 0;
}
#endif
