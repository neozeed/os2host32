#define INCL_32
#define INCL_DOSPROCESS
#include <os2.h>
#include <stdio.h>
static volatile int finished;
static void worker(void)
{
    DosSleep(30UL);
    finished = 1;
    DosExit(EXIT_THREAD, 0UL);
}
int main(void)
{
    TID tid;
    ULONG rc;
    rc = DosCreateThread(&tid, worker, 0UL, 0UL, 8192UL);
    if (rc) { printf("THREAD-R25 create failed rc=%lu\n", rc); return 1; }
    rc = DosWaitThread(&tid, DCWW_WAIT);
    printf("THREAD-R25 wait rc=%lu completed=%d\n", rc, finished);
    return rc || !finished ? 1 : 0;
}
