#define TEST_NAME "sleep1"
#include "r8test.h"
static volatile ULONG steps;
static void worker(ULONG arg)
{
    ULONG i;
    (void)arg;
    for(i=0;i<100;i++) { ++steps; CHECK(DosSleep(0)==0); }
}
int main(void)
{
    TID tid;
    ULONG start,end;
    HEV event;
    CHECK(DosSleep(0)==0); /* lone runnable thread */
    CHECK(DosCreateThread(&tid,(PFNTHREAD)worker,0,0,16384)==0);
    CHECK(steps==0);
    CHECK(DosSleep(0)==0);
    CHECK(steps>0);
    CHECK(DosQuerySysInfo(QSV_MS_COUNT,QSV_MS_COUNT,&start,sizeof(start))==0);
    CHECK(DosSleep(30)==0);
    CHECK(DosQuerySysInfo(QSV_MS_COUNT,QSV_MS_COUNT,&end,sizeof(end))==0);
    CHECK(end-start>=30); /* no tight upper limit on a busy Windows host */
    CHECK(DosWaitThread(&tid,DCWW_WAIT)==0);
    CHECK(steps==100);
    CHECK(DosCreateEventSem(NULL,&event,0,FALSE)==0);
    CHECK(DosWaitEventSem(event,10)==121);
    CHECK(DosCloseEventSem(event)==0);
    printf("sleep1 PASS\n");return 0;
}
