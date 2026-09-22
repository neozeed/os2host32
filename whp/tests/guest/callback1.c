#define TEST_NAME "callback1"
#include "cbtest.h"
static ULONG tibs[5];
static volatile ULONG done[4],exit_mark;
static HEV gate;
/* Deliberately violates callee-saved registers and sets DF. The host must
 * restore its caller context rather than trusting callback ABI behaviour. */
static unsigned char clobber_code[]={
    0xbb,0x11,0x11,0x11,0x11, 0xbe,0x22,0x22,0x22,0x22,
    0xbf,0x33,0x33,0x33,0x33, 0xbd,0x44,0x44,0x44,0x44,
    0x8b,0x44,0x24,0x04, 0x33,0x44,0x24,0x08, 0xfd,0xc3
};
static ULONG _cdecl arithmetic(ULONG a,ULONG b) { return (a*3UL)^b; }
static ULONG _cdecl nested(ULONG id,ULONG depth)
{
    ULONG value;
    volatile ULONG guard;
    guard=0xcafe0000UL+id;
    CHECK(id<5 && fs_tib2()==tibs[id]);
    CHECK(DosSleep(0)==0);
    if(depth) {
        CHECK(HostInvoke(nested,id,depth-1,&value)==0);
        ++value;
    } else value=0x1000UL+id;
    CHECK(guard==0xcafe0000UL+id && fs_tib2()==tibs[id]);
    return value;
}
static void worker(ULONG id)
{
    ULONG i,result;
    tibs[id]=fs_tib2();
    for(i=0;i<20;i++) {
        CHECK(HostInvoke(nested,id,3,&result)==0 && result==0x1003UL+id);
        CHECK(fs_tib2()==tibs[id]);
    }
    done[id-1]=1;
}
static ULONG _cdecl waiting(ULONG a,ULONG b)
{
    CHECK(DosWaitEventSem(gate,1000)==0);
    CHECK(fs_tib2()==tibs[0]);
    return a+b;
}
static void poster(ULONG ignored)
{ (void)ignored;CHECK(DosSleep(5)==0);CHECK(DosPostEventSem(gate)==0); }
static ULONG _cdecl exiting(ULONG a,ULONG b)
{ (void)a;(void)b;exit_mark=1;DosExit(EXIT_THREAD,0);return 0; }
static void exit_worker(ULONG ignored)
{
    ULONG value;
    (void)ignored;
    HostInvoke(exiting,0,0,&value);
    CHECK(0); /* must never resume this abandoned continuation */
}
static ULONG _cdecl depth_limit(ULONG a,ULONG depth)
{
    ULONG value,rc;
    (void)a;
    rc=HostInvoke(depth_limit,0,depth+1,&value);
    if(depth==15) { CHECK(rc==8);return 1; }
    CHECK(rc==0);return value+1;
}
int main(void)
{
    ULONG value,i;
    TID tids[4],tid;
    union { unsigned char *data; CALLBACK2 call; } probe;
    tibs[0]=fs_tib2();
    value=0xdeadbeefUL;
    CHECK(HostInvoke((CALLBACK2)0,0,0,&value)==87 && value==0xdeadbeefUL);
    CHECK(HostInvoke(arithmetic,0,0,NULL)==87);
    CHECK(HostInvoke(arithmetic,0x80000000UL,0x12345678UL,&value)==0);
    CHECK(value==0x92345678UL);
    probe.data=clobber_code;
    CHECK(HostInvoke(probe.call,0x11223344UL,0xaabbccddUL,&value)==0);
    CHECK(value==0xbb99ff99UL);
    CHECK(HostInvoke(depth_limit,0,0,&value)==0 && value==16);
    for(i=0;i<4;i++) CHECK(DosCreateThread(&tids[i],(PFNTHREAD)worker,i+1,0,32768)==0);
    CHECK(HostInvoke(nested,0,4,&value)==0 && value==0x1004UL);
    for(i=0;i<4;i++) {tid=tids[i];CHECK(DosWaitThread(&tid,DCWW_WAIT)==0 && done[i]==1);}
    for(i=0;i<4;i++) CHECK(tibs[i+1]!=tibs[0]);
    CHECK(DosCreateEventSem(NULL,&gate,0,FALSE)==0);
    CHECK(DosCreateThread(&tid,(PFNTHREAD)poster,0,0,16384)==0);
    CHECK(HostInvoke(waiting,20,22,&value)==0 && value==42);
    CHECK(DosWaitThread(&tid,DCWW_WAIT)==0);
    CHECK(DosCloseEventSem(gate)==0);
    for(i=0;i<40;i++) {
        exit_mark=0;
        CHECK(DosCreateThread(&tid,(PFNTHREAD)exit_worker,0,0,16384)==0);
        CHECK(DosWaitThread(&tid,DCWW_WAIT)==0 && exit_mark==1);
    }
    CHECK(HostInvoke(arithmetic,7,9,&value)==0 && value==28);
    CHECK(fs_tib2()==tibs[0]);
    printf("callback1 PASS\n");return 0;
}
