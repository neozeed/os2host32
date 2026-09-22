#define TEST_NAME "queue1"
#include "r8test.h"
static HQUEUE queue;
static char name[]="\\QUEUES\\WHP_R8_QUEUE";
static volatile ULONG received[3],closed_rc;
static void reader(ULONG index)
{
    REQUESTDATA req;
    ULONG len;
    PVOID data;
    BYTE priority;
    CHECK(DosReadQueue(queue,&req,&len,&data,0,DCWW_WAIT,&priority,(HSEM)0)==0);
    CHECK(req.pid==1 && len==77 && (ULONG)data==0x12340000UL+req.ulData);
    CHECK(priority==3);received[index]=req.ulData;
}
static void closed_reader(ULONG arg)
{
    REQUESTDATA req;ULONG len;PVOID data;BYTE priority;
    (void)arg;
    closed_rc=DosReadQueue(queue,&req,&len,&data,0,DCWW_WAIT,&priority,(HSEM)0);
}
static ULONG read_request(void)
{
    REQUESTDATA req;ULONG len;PVOID data;BYTE priority;
    CHECK(DosReadQueue(queue,&req,&len,&data,0,DCWW_NOWAIT,&priority,(HSEM)0)==0);
    return req.ulData;
}
int main(void)
{
    TID tids[3],tid;
    PID pid;
    HQUEUE other,stale;
    ULONG count,i,token,len;
    REQUESTDATA req;
    PVOID data;
    BYTE priority;
    HEV event;
    CHECK(sizeof(REQUESTDATA)==8);
    CHECK(DosCreateQueue(&queue,QUE_FIFO,(PSZ)name)==0);
    CHECK(DosCreateQueue(&other,QUE_FIFO,(PSZ)name)==332);
    CHECK(DosOpenQueue(&pid,&other,(PSZ)name)==0 && pid==1 && other==queue);
    CHECK(DosCloseQueue(other)==0);
    CHECK(DosReadQueue(queue,&req,&len,&data,0,DCWW_NOWAIT,&priority,(HSEM)0)==342);
    CHECK(DosWriteQueue(queue,11,2,(PVOID)0xdeadbeefUL,1)==0);
    CHECK(DosWriteQueue(queue,22,4,(PVOID)0xcafebabeUL,2)==0);
    token=0;
    CHECK(DosPeekQueue(queue,&req,&len,&data,&token,DCWW_NOWAIT,&priority,(HSEM)0)==0);
    CHECK(req.ulData==11 && token!=0 && (ULONG)data==0xdeadbeefUL && len==2);
    CHECK(DosQueryQueue(queue,&count)==0 && count==2);
    CHECK(DosReadQueue(queue,&req,&len,&data,token,DCWW_NOWAIT,&priority,(HSEM)0)==0);
    CHECK(req.ulData==11 && read_request()==22);
    CHECK(DosCreateEventSem(NULL,&event,0,FALSE)==0);
    CHECK(DosReadQueue(queue,&req,&len,&data,0,DCWW_NOWAIT,&priority,(HSEM)event)==342);
    CHECK(DosWriteQueue(queue,99,0,NULL,0)==0);
    CHECK(DosWaitEventSem(event,0)==0);
    CHECK(DosResetEventSem(event,&count)==0 && count==1);
    CHECK(DosPurgeQueue(queue)==0);
    CHECK(DosQueryQueue(queue,&count)==0 && count==0);
    for(i=0;i<3;i++) CHECK(DosCreateThread(&tids[i],(PFNTHREAD)reader,i,0,16384)==0);
    CHECK(DosSleep(0)==0); /* all three readers must block, letting main run */
    for(i=0;i<3;i++) CHECK(received[i]==0);
    for(i=0;i<3;i++) CHECK(DosWriteQueue(queue,i+1,77,(PVOID)(0x12340001UL+i),3)==0);
    for(i=0;i<3;i++) {tid=tids[i];CHECK(DosWaitThread(&tid,DCWW_WAIT)==0);CHECK(received[i]==i+1);}
    CHECK(DosCreateThread(&tid,(PFNTHREAD)closed_reader,0,0,16384)==0);
    CHECK(DosSleep(0)==0);
    stale=queue;CHECK(DosCloseQueue(queue)==0);
    CHECK(DosWaitThread(&tid,DCWW_WAIT)==0 && closed_rc==337);
    CHECK(DosCloseEventSem(event)==0);
    CHECK(DosCreateQueue(&queue,QUE_LIFO,(PSZ)name)==0 && queue!=stale);
    CHECK(DosQueryQueue(stale,&count)==337);
    CHECK(DosWriteQueue(queue,1,0,NULL,0)==0);CHECK(DosWriteQueue(queue,2,0,NULL,0)==0);
    CHECK(read_request()==2 && read_request()==1);CHECK(DosCloseQueue(queue)==0);
    CHECK(DosCreateQueue(&queue,QUE_PRIORITY,(PSZ)name)==0);
    CHECK(DosWriteQueue(queue,1,0,NULL,1)==0);
    CHECK(DosWriteQueue(queue,2,0,NULL,15)==0);
    CHECK(DosWriteQueue(queue,3,0,NULL,15)==0);
    CHECK(read_request()==2 && read_request()==3 && read_request()==1);
    CHECK(DosCloseQueue(queue)==0);
    printf("queue1 PASS\n");return 0;
}
