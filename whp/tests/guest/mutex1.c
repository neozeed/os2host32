#define TEST_NAME "mutex1"
#include "r8test.h"
static HMTX mutex;
static volatile ULONG stage,counter,inside;
static char name[]="\\SEM32\\WHP_R8_MUTEX";
static void timeout_worker(ULONG arg)
{
    (void)arg;
    CHECK(DosReleaseMutexSem(mutex)==288);
    CHECK(DosRequestMutexSem(mutex,0)==121);
    CHECK(DosRequestMutexSem(mutex,10)==121);
    stage=1;
    CHECK(DosRequestMutexSem(mutex,FOREVER)==0);
    stage=2;
    CHECK(DosReleaseMutexSem(mutex)==0);
}
static void counter_worker(ULONG arg)
{
    ULONG i,value;
    (void)arg;
    for(i=0;i<50;i++) {
        CHECK(DosRequestMutexSem(mutex,FOREVER)==0);
        CHECK(inside==0);inside=1;
        value=counter;
        CHECK(DosSleep(0)==0); /* force contention while owning the mutex */
        CHECK(inside==1);counter=value+1;inside=0;
        CHECK(DosReleaseMutexSem(mutex)==0);
    }
}
static void abandon_worker(ULONG arg)
{
    (void)arg;
    CHECK(DosRequestMutexSem(mutex,FOREVER)==0);
    CHECK(DosRequestMutexSem(mutex,0)==0);
    /* Return owning the recursive mutex. Main must receive OWNER_DIED. */
}
int main(void)
{
    TID tid,tids[4],owner;
    PID pid;
    ULONG count,i;
    HMTX other,stale;
    CHECK(DosCreateMutexSem((PSZ)name,&mutex,0,TRUE)==0);
    CHECK(DosCreateMutexSem((PSZ)name,&other,0,FALSE)==285);
    CHECK(DosOpenMutexSem((PSZ)name,&other)==0 && other==mutex);
    CHECK(DosCloseMutexSem(other)==0);
    CHECK(DosRequestMutexSem(mutex,0)==0);
    CHECK(DosQueryMutexSem(mutex,&pid,&owner,&count)==0);
    CHECK(pid==1 && owner==1 && count==2);
    CHECK(DosCloseMutexSem(mutex)==301);
    CHECK(DosCreateThread(&tid,(PFNTHREAD)timeout_worker,0,0,16384)==0);
    while(stage==0) CHECK(DosSleep(1)==0);
    CHECK(DosReleaseMutexSem(mutex)==0);
    CHECK(DosQueryMutexSem(mutex,&pid,&owner,&count)==0 && count==1);
    CHECK(DosReleaseMutexSem(mutex)==0);
    CHECK(DosRequestMutexSem(mutex,0)==121); /* ownership reserved to waiter */
    CHECK(DosWaitThread(&tid,DCWW_WAIT)==0 && stage==2);
    for(i=0;i<4;i++) CHECK(DosCreateThread(&tids[i],(PFNTHREAD)counter_worker,i,0,16384)==0);
    for(i=0;i<4;i++) { tid=tids[i];CHECK(DosWaitThread(&tid,DCWW_WAIT)==0); }
    CHECK(counter==200 && inside==0);
    CHECK(DosCreateThread(&tid,(PFNTHREAD)abandon_worker,0,0,16384)==0);
    CHECK(DosWaitThread(&tid,DCWW_WAIT)==0);
    CHECK(DosRequestMutexSem(mutex,0)==105);
    CHECK(DosQueryMutexSem(mutex,&pid,&owner,&count)==0 && owner==1 && count==1);
    CHECK(DosReleaseMutexSem(mutex)==0);
    stale=mutex;CHECK(DosCloseMutexSem(mutex)==0);
    CHECK(DosCreateMutexSem(NULL,&mutex,0,FALSE)==0 && mutex!=stale);
    CHECK(DosRequestMutexSem(stale,0)==6);
    CHECK(DosCloseMutexSem(mutex)==0);
    printf("mutex1 PASS\n");return 0;
}
