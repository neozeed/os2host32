/*
 * thread1.c - minimal OS/2 2.x 32-bit C/386 V2 threading regression.
 */
#define INCL_DOSPROCESS
#define INCL_DOSFILEMGR
#include <os2.h>

static void worker(ULONG arg)
{
    static char msg[] = "Hello from worker thread!\r\n";
    ULONG written;

    (void)arg;
    written = 0;
    DosWrite(1, msg, sizeof(msg) - 1, &written);
    DosExit(EXIT_THREAD, 0);
}

int main(void)
{
    TID tid;
    APIRET rc;

    tid = 0;
    rc = DosCreateThread(&tid, worker, 0, 0, 16384);
    if (rc != 0)
        return (int)rc;

    rc = DosWaitThread(&tid, DCWW_WAIT);
    if (rc != 0)
        return (int)rc;

    return 0;
}
