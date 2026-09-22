/*
 * upper-dos-test.c - M28D5 pipeline filter using only the OS/2 HFILE API.
 *
 * This deliberately avoids C stdio.  It lets us validate DosRead/DosWrite,
 * inherited HFILE 0/1, asynchronous children and pipeline EOF independently
 * of the recovered C/386 CRT's FILE buffering behaviour.
 */
#define INCL_DOSFILEMGR
#include <os2.h>

static unsigned char up(unsigned char c)
{
    if (c >= (unsigned char)'a' && c <= (unsigned char)'z')
        return (unsigned char)(c - (unsigned char)'a' + (unsigned char)'A');
    return c;
}

int main(void)
{
    unsigned char buf[256];
    ULONG got;
    ULONG put;
    ULONG off;
    APIRET rc;

    for (;;) {
        got = 0;
        rc = DosRead((HFILE)0, buf, (ULONG)sizeof(buf), &got);
        if (rc != 0)
            return 1;
        if (got == 0)
            break;

        for (off = 0; off < got; ++off)
            buf[off] = up(buf[off]);

        off = 0;
        while (off < got) {
            put = 0;
            rc = DosWrite((HFILE)1, buf + off, got - off, &put);
            if (rc != 0 || put == 0)
                return 2;
            off += put;
        }
    }
    return 0;
}
