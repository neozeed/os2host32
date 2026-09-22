/*
 * dllthread.c - WHP V2 guest-DLL/thread sharing torture test.
 * Default 16 workers, optional argv[1] 1..30.
 *
 * Every worker executes code in the same mapped V2DLL.DLL.  V2DllNext()
 * increments DLL instance data, proving that the DLL's data belongs to the
 * process rather than to an individual virtual thread.
 */
#define INCL_DOSPROCESS
#define INCL_DOSFILEMGR
#include <os2.h>
#include <stdlib.h>

#define DEFAULT_WORKERS 16
#define MAX_WORKERS     30
#define WORKER_STACK    8192

extern ULONG V2DllAdd(ULONG, ULONG);
extern ULONG V2DllNext(void);
extern ULONG V2DllSay(char *, ULONG);

static TID tids[MAX_WORKERS];
static volatile ULONG seq[MAX_WORKERS];
static volatile ULONG sums[MAX_WORKERS];

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

    i = arg;
    seq[i] = V2DllNext();
    sums[i] = V2DllAdd(0x1000UL, i);

    if ((i & 1UL) == 0)
        DosExit(EXIT_THREAD, 0);
    /* Odd workers return through the V2 private thread-return veneer. */
}

int main(int argc, char **argv)
{
    int nworkers;
    int i;
    APIRET rc;
    TID t;
    static char dllmsg[] = "dllthread: message emitted inside V2DLL.DLL\r\n";

    nworkers = DEFAULT_WORKERS;
    if (argc > 1) {
        nworkers = atoi(argv[1]);
        if (nworkers < 1)
            nworkers = 1;
        if (nworkers > MAX_WORKERS)
            nworkers = MAX_WORKERS;
    }

    say("dllthread: creating workers\r\n");

    for (i = 0; i < nworkers; ++i) {
        tids[i] = 0;
        seq[i] = 0;
        sums[i] = 0;
        rc = DosCreateThread(&tids[i], worker, (ULONG)i, 0, WORKER_STACK);
        if (rc != 0)
            return (int)rc;
    }

    for (i = 0; i < nworkers; ++i) {
        t = tids[i];
        rc = DosWaitThread(&t, DCWW_WAIT);
        if (rc != 0)
            return (int)rc;
    }

    for (i = 0; i < nworkers; ++i) {
        if (seq[i] != (ULONG)(i + 1)) {
            say("dllthread: shared DLL counter mismatch\r\n");
            return 80;
        }
        if (sums[i] != 0x1000UL + (ULONG)i) {
            say("dllthread: DLL arithmetic mismatch\r\n");
            return 81;
        }
    }

    rc = V2DllSay(dllmsg, sizeof(dllmsg) - 1UL);
    if (rc != 0)
        return (int)rc;

    say("dllthread PASS\r\n");
    return 0;
}
