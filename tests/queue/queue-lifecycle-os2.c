/* C/386 target smoke: run unchanged through native host and soft386. */
#define INCL_32
#define INCL_DOSPROCESS
#define INCL_DOSQUEUES
#include <os2.h>
#include <stdio.h>
static int failures;
static void check(int ok, const char *name)
{
    printf("QUEUE-R24 %s: %s\n", name, ok ? "PASS" : "FAIL");
    if (!ok) ++failures;
}
int main(void)
{
    HQUEUE q, old;
    ULONG rc, count, length;
    REQUESTDATA request;
    PVOID value;
    BYTE priority;
    rc = DosCreateQueue(&q, QUE_FIFO, "\\QUEUES\\R24_SMOKE");
    check(rc == 0, "create");
    if (rc) return 1;
    check(DosWriteQueue(q, 123UL, 4UL, (PVOID)440UL, 0UL) == 0, "opaque integer write");
    count = 99;
    check(DosQueryQueue(q, &count) == 0 && count == 1, "query one");
    check(DosPurgeQueue(q) == 0, "purge");
    check(DosQueryQueue(q, &count) == 0 && count == 0, "query empty");
    value = 0; length = 0; priority = 0;
    check(DosReadQueue(q, &request, &length, &value, 0UL, DCWW_NOWAIT,
                       &priority, 0UL) == 342UL, "read empty");
    old = q;
    check(DosCloseQueue(q) == 0, "close");
    count = 77;
    check(DosQueryQueue(old, &count) == 337UL && count == 77, "closed query");
    rc = DosCreateQueue(&q, QUE_FIFO, "\\QUEUES\\R24_SMOKE");
    check(rc == 0, "recreate same name");
    if (!rc) {
        check(DosCloseQueue(old) == 337UL, "stale handle rejected");
        check(DosCloseQueue(q) == 0, "close replacement");
    }
    return failures ? 1 : 0;
}

