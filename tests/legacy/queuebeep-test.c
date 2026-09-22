/*
 * queuebeep-test.c
 *
 * Small Microsoft C/386 / OS/2 test for the exact path used by the old
 * Sarien beeper hack:
 *
 *   DosCreateQueue -> DosCreateThread -> DosOpenQueue ->
 *   DosWriteQueue -> DosReadQueue -> DosBeep
 *
 * After building the LE with C/386, translate it with le2pe386 and run it
 * beside milestone-15 DOSCALLS.DLL and QUECALLS.DLL.
 */
#define INCL_DOS
#include <os2.h>
#include <stdio.h>

static HQUEUE g_queue;
static PSZ g_name = "\\QUEUES\\BEEPTEST.QUE";

static void beep_thread(ULONG ignored)
{
    PID owner;
    HQUEUE q;
    REQUESTDATA request;
    ULONG cb;
    PVOID data;
    BYTE priority;
    APIRET rc;

    ignored = ignored;
    owner = 0;
    q = 0;
    cb = 0;
    data = 0;
    priority = 0;

    rc = DosOpenQueue(&owner, &q, g_name);
    printf("DosOpenQueue rc=%lu q=%lu owner=%lu\n",
           (unsigned long)rc, (unsigned long)q, (unsigned long)owner);
    if (rc != 0)
        return;

    rc = DosReadQueue(q, &request, &cb, &data,
                      0UL, DCWW_WAIT, &priority, 0UL);
    printf("DosReadQueue rc=%lu cb=%lu data=%p\n",
           (unsigned long)rc, (unsigned long)cb, data);
    if (rc == 0) {
        printf("beeping value %lu Hz\n", (unsigned long)data);
        DosBeep((ULONG)data, 750UL);
    }
}

int main(void)
{
    TID tid;
    APIRET rc;

    g_queue = 0;
    rc = DosCreateQueue(&g_queue, QUE_FIFO, g_name);
    printf("DosCreateQueue rc=%lu q=%lu\n",
           (unsigned long)rc, (unsigned long)g_queue);
    if (rc != 0)
        return 1;

    rc = DosCreateThread(&tid, beep_thread, 0UL, 0UL, 16384UL);
    printf("DosCreateThread rc=%lu tid=%lu\n",
           (unsigned long)rc, (unsigned long)tid);
    if (rc != 0)
        return 1;

    DosSleep(100UL);

    /* Deliberately use the same pointer-as-value trick as Sarien. */
    rc = DosWriteQueue(g_queue, 12345UL, sizeof(int), (PVOID)440UL, 0UL);
    printf("DosWriteQueue rc=%lu\n", (unsigned long)rc);

    DosSleep(1500UL);
    return 0;
}
