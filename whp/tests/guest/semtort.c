/*
 * semtort.c
 *
 * C/386 event-semaphore scheduler torture test.
 * Default: 16 blocked worker threads, each on its own HEV.
 * Optional argv[1]: 1..24 workers.
 *
 * A final controller thread posts all worker events only after the main thread
 * has blocked.  With the V2 round-robin scheduler this causes each worker to
 * run, block in DosWaitEventSem, and leave the controller as the last runnable
 * guest thread.  The controller then wakes the whole set.
 */

#define INCL_DOSPROCESS
#define INCL_DOSFILEMGR
#define INCL_DOSSEMAPHORES
#include <os2.h>
#include <stdlib.h>

#ifndef ERROR_SEM_TIMEOUT
#define ERROR_SEM_TIMEOUT 121
#endif
#ifndef ERROR_ALREADY_POSTED
#define ERROR_ALREADY_POSTED 299
#endif
#ifndef SEM_INDEFINITE_WAIT
#define SEM_INDEFINITE_WAIT 0xffffffffUL
#endif

#define DEFAULT_WORKERS 16
#define MAX_WORKERS     24
#define WORKER_STACK    8192

static HEV events[MAX_WORKERS];
static TID tids[MAX_WORKERS];
static volatile ULONG marks[MAX_WORKERS];
static ULONG nworkers_global;

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
    ULONG i;
    APIRET rc;

    i = arg;
    rc = DosWaitEventSem(events[i], SEM_INDEFINITE_WAIT);
    if (rc != 0) {
        marks[i] = 0xBAD00000UL + rc;
        DosExit(EXIT_THREAD, rc);
    }

    marks[i] = 0x5EAD0000UL + i;
    DosExit(EXIT_THREAD, 0);
}

static void controller(ULONG arg)
{
    LONG i;
    APIRET rc;

    (void)arg;
    say("semtort controller: posting events\r\n");

    /* Reverse order makes handle/index mistakes obvious in the host trace. */
    i = (LONG)nworkers_global - 1;
    while (i >= 0) {
        rc = DosPostEventSem(events[i]);
        if (rc != 0)
            DosExit(EXIT_THREAD, rc);
        --i;
    }

    /* Deliberately post event 0 twice.  OS/2 increments the post count and
       reports ERROR_ALREADY_POSTED on the second post. */
    rc = DosPostEventSem(events[0]);
    if (rc != ERROR_ALREADY_POSTED)
        DosExit(EXIT_THREAD, rc ? rc : 98);

    DosExit(EXIT_THREAD, 0);
}

int main(int argc, char **argv)
{
    int nworkers;
    int i;
    APIRET rc;
    TID controller_tid;
    TID t;
    ULONG count;

    nworkers = DEFAULT_WORKERS;
    if (argc > 1) {
        nworkers = atoi(argv[1]);
        if (nworkers < 1)
            nworkers = 1;
        if (nworkers > MAX_WORKERS)
            nworkers = MAX_WORKERS;
    }
    nworkers_global = (ULONG)nworkers;

    say("semtort: creating event semaphores and workers\r\n");

    for (i = 0; i < nworkers; ++i) {
        events[i] = 0;
        tids[i] = 0;
        marks[i] = 0;
        rc = DosCreateEventSem(NULL, &events[i], 0, FALSE);
        if (rc != 0)
            return (int)rc;
        rc = DosCreateThread(&tids[i], worker, (ULONG)i, 0, WORKER_STACK);
        if (rc != 0)
            return (int)rc;
    }

    /* Verify the immediate timeout path before anything is posted. */
    rc = DosWaitEventSem(events[0], 0);
    if (rc != ERROR_SEM_TIMEOUT)
        return 70;

    controller_tid = 0;
    rc = DosCreateThread(&controller_tid, controller, 0, 0, WORKER_STACK);
    if (rc != 0)
        return (int)rc;

    /* This blocks main.  Scheduler order then runs each worker until it blocks
       on its HEV, finally reaching the controller which posts the events. */
    t = tids[0];
    rc = DosWaitThread(&t, DCWW_WAIT);
    if (rc != 0)
        return (int)rc;

    for (i = 0; i < nworkers; ++i) {
        t = tids[i];
        rc = DosWaitThread(&t, DCWW_WAIT);
        if (rc != 0)
            return (int)rc;
        if (marks[i] != 0x5EAD0000UL + (ULONG)i)
            return 71;
    }

    t = controller_tid;
    rc = DosWaitThread(&t, DCWW_WAIT);
    if (rc != 0)
        return (int)rc;

    for (i = 0; i < nworkers; ++i) {
        count = 0;
        rc = DosQueryEventSem(events[i], &count);
        if (rc != 0)
            return (int)rc;
        if (count != (i == 0 ? 2UL : 1UL))
            return 72;

        count = 0;
        rc = DosResetEventSem(events[i], &count);
        if (rc != 0)
            return (int)rc;
        if (count != (i == 0 ? 2UL : 1UL))
            return 73;

        rc = DosCloseEventSem(events[i]);
        if (rc != 0)
            return (int)rc;
    }

    say("semtort PASS\r\n");
    return 0;
}
