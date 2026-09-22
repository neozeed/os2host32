#define TEST_NAME "recycle1"
#include "r8test.h"
static volatile ULONG done;
static void worker(ULONG arg)
{
    volatile ULONG stack_guard[16];
    stack_guard[0]=arg;stack_guard[15]=~arg;
    CHECK(DosSleep(0)==0);
    CHECK(stack_guard[0]==arg && stack_guard[15]==~arg);
    done=arg;
    if(arg&1UL) DosExit(EXIT_THREAD,0);
}
int main(int argc,char **argv)
{
    ULONG n,i,v;
    TID tid,previous,first;
    char *s;
    n=1000;previous=0;first=0;
    CHECK(argc<=2);
    if(argc==2) {
        s=argv[1];n=0;CHECK(*s!=0);
        while(*s) {CHECK(*s>='0' && *s<='9');v=(ULONG)(*s++-'0');CHECK(n<=(1000000UL-v)/10UL);n=n*10+v;}
        CHECK(n>0 && n<=1000000UL);
    }
    for(i=1;i<=n;i++) {
        CHECK(DosCreateThread(&tid,(PFNTHREAD)worker,i,0,65536)==0);
        CHECK(tid>previous);previous=tid;if(i==1)first=tid;
        CHECK(DosWaitThread(&tid,DCWW_WAIT)==0 && done==i);
    }
    /* R8 keeps the earlier late/repeated-wait compatibility by never reusing
       issued TIDs, even though their slots/stacks have been recycled. */
    CHECK(DosWaitThread(&first,DCWW_NOWAIT)==0);
    printf("recycle1 PASS (%lu threads in one process)\n",n);return 0;
}
