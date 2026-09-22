/*
 * threadtort.c
 *
 * Microsoft C/386 / OS/2 2.x thread scheduler torture test.
 * Default: 16 workers.  Optional argv[1]: 1..30 workers.
 *
 * Deliberately uses only simple OS/2 APIs already on the V2 path.
 */

#define INCL_DOSPROCESS
#define INCL_DOSFILEMGR
#include <os2.h>
#include <stdlib.h>

#define DEFAULT_WORKERS 16
#define MAX_WORKERS     30
#define WORKER_STACK    8192

static TID tids[MAX_WORKERS];
static volatile ULONG marks[MAX_WORKERS];

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

static char *put_uint(char *p, ULONG v)
{
    char tmp[16];
    int n;

    n = 0;
    do {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    } while (v != 0);

    while (n != 0)
        *p++ = tmp[--n];
    return p;
}

static void worker(ULONG arg)
{
    ULONG i;
    char msg[64];
    char *p;
    ULONG written;

    i = arg;
    p = msg;
    *p++ = 'w'; *p++ = 'o'; *p++ = 'r'; *p++ = 'k'; *p++ = 'e'; *p++ = 'r';
    *p++ = ' ';
    p = put_uint(p, i);
    *p++ = ' '; *p++ = 'r'; *p++ = 'u'; *p++ = 'n';
    *p++ = '\r'; *p++ = '\n';
    *p = '\0';

    written = 0;
    DosWrite(1, msg, (ULONG)(p - msg), &written);
    marks[i] = 0xC3860000UL + i;

    /* Exercise both thread-termination paths. */
    if ((i & 1UL) == 0)
        DosExit(EXIT_THREAD, 0);

    /* Odd workers deliberately fall through to a normal C return. */
}

int main(int argc, char **argv)
{
    int nworkers;
    int i;
    APIRET rc;
    TID t;

    nworkers = DEFAULT_WORKERS;
    if (argc > 1) {
        nworkers = atoi(argv[1]);
        if (nworkers < 1)
            nworkers = 1;
        if (nworkers > MAX_WORKERS)
            nworkers = MAX_WORKERS;
    }

    say("thread torture: creating workers\r\n");

    for (i = 0; i < nworkers; ++i) {
        tids[i] = 0;
        marks[i] = 0;
        rc = DosCreateThread(&tids[i], worker, (ULONG)i, 0, WORKER_STACK);
        if (rc != 0) {
            say("thread torture: DosCreateThread failed\r\n");
            return (int)rc;
        }
    }

    /* Waiting for worker 0 forces the V2 scheduler to start draining the
       runnable worker set.  Subsequent waits should mostly hit DEAD threads. */
    for (i = 0; i < nworkers; ++i) {
        t = tids[i];
        rc = DosWaitThread(&t, DCWW_WAIT);
        if (rc != 0) {
            say("thread torture: DosWaitThread failed\r\n");
            return (int)rc;
        }
    }

    for (i = 0; i < nworkers; ++i) {
        if (marks[i] != 0xC3860000UL + (ULONG)i) {
            say("thread torture: completion mark mismatch\r\n");
            return 90;
        }
    }

    say("thread torture PASS\r\n");
    return 0;
}
