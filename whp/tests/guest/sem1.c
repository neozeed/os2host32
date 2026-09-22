/*
 * sem1.c
 *
 * Minimal C/386 event-semaphore + thread blocking test for WHP OS/2 V2.
 */

#define INCL_DOSPROCESS
#define INCL_DOSFILEMGR
#define INCL_DOSSEMAPHORES
#include <os2.h>

#ifndef ERROR_SEM_TIMEOUT
#define ERROR_SEM_TIMEOUT 121
#endif
#ifndef SEM_INDEFINITE_WAIT
#define SEM_INDEFINITE_WAIT 0xffffffffUL
#endif

static void say(const char *s)
{
    ULONG n;
    ULONG written;

    n = 0;
    while (s[n] != '\0')
        ++n;
    written = 0;
    DosWrite(1, (PVOID)s, n, &written);
}

static void worker(ULONG arg)
{
    HEV hev;
    APIRET rc;

    hev = (HEV)arg;
    say("sem1 worker: posting event\r\n");
    rc = DosPostEventSem(hev);
    if (rc != 0)
        say("sem1 worker: DosPostEventSem failed\r\n");
    DosExit(EXIT_THREAD, (ULONG)rc);
}

int main(void)
{
    HEV hev;
    TID tid;
    TID wait_tid;
    ULONG count;
    APIRET rc;

    hev = 0;
    rc = DosCreateEventSem(NULL, &hev, 0, FALSE);
    if (rc != 0)
        return (int)rc;

    count = 0xdeadbeefUL;
    rc = DosQueryEventSem(hev, &count);
    if (rc != 0 || count != 0)
        return 20;

    /* An unposted semaphore with an immediate timeout must time out. */
    rc = DosWaitEventSem(hev, 0);
    if (rc != ERROR_SEM_TIMEOUT)
        return 21;

    tid = 0;
    rc = DosCreateThread(&tid, worker, (ULONG)hev, 0, 8192);
    if (rc != 0)
        return (int)rc;

    say("sem1 main: waiting for event\r\n");
    rc = DosWaitEventSem(hev, SEM_INDEFINITE_WAIT);
    if (rc != 0)
        return (int)rc;
    say("sem1 main: awakened\r\n");

    count = 0;
    rc = DosQueryEventSem(hev, &count);
    if (rc != 0 || count != 1)
        return 22;

    count = 0;
    rc = DosResetEventSem(hev, &count);
    if (rc != 0 || count != 1)
        return 23;

    count = 0xdeadbeefUL;
    rc = DosQueryEventSem(hev, &count);
    if (rc != 0 || count != 0)
        return 24;

    /* With the worker now gone and no other runnable guest thread, this
       specifically tests the host scheduler's finite-timeout wakeup path. */
    say("sem1 main: testing 25ms timeout\r\n");
    rc = DosWaitEventSem(hev, 25);
    if (rc != ERROR_SEM_TIMEOUT)
        return 25;

    wait_tid = tid;
    rc = DosWaitThread(&wait_tid, DCWW_WAIT);
    if (rc != 0)
        return (int)rc;

    rc = DosCloseEventSem(hev);
    if (rc != 0)
        return (int)rc;

    say("sem1 PASS\r\n");
    return 0;
}
