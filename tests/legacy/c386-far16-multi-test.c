/*
 * c386-far16-multi-test.c - M29F multiple Microsoft C/386 far16 thunks.
 *
 * One genuine 32-bit C/386 LE imports four classic 16-bit console APIs at
 * once.  The purpose is to prove that LINK386 can place several migration
 * thunk fragments in one 16-bit object and that OS2HOST32 can pair, validate,
 * and replace each of them independently without executing 286 code.
 */

#include <stdio.h>

typedef unsigned char  UCHAR;
typedef unsigned short USHORT;
typedef unsigned long  ULONG;

#pragma pack(2)
typedef struct _KBDKEYINFO {
    UCHAR  chChar;
    UCHAR  chScan;
    UCHAR  fbStatus;
    UCHAR  bNlsShift;
    USHORT fsState;
    ULONG  time;
} KBDKEYINFO;

typedef struct _STRINGINBUF {
    USHORT cb;
    USHORT cchIn;
} STRINGINBUF;
#pragma pack()

typedef char KBDKEYINFO_must_be_10_bytes[(sizeof(KBDKEYINFO) == 10) ? 1 : -1];
typedef char STRINGINBUF_must_be_4_bytes[(sizeof(STRINGINBUF) == 4) ? 1 : -1];

USHORT _far16 _pascal VIOWRTTTY(char _far16 *text,
                                USHORT count,
                                USHORT hvio);
USHORT _far16 _pascal KBDCHARIN(KBDKEYINFO _far16 *info,
                                USHORT wait,
                                USHORT hkbd);
USHORT _far16 _pascal KBDSTRINGIN(char _far16 *buffer,
                                  STRINGINBUF _far16 *length,
                                  USHORT wait,
                                  USHORT hkbd);
USHORT _far16 _pascal KBDFLUSHBUFFER(USHORT hkbd);

static USHORT tty(char *s, USHORT n)
{
    return VIOWRTTTY(s, n, (USHORT)0);
}

int main(void)
{
    static char intro[] =
        "M29F multi-thunk: press one key, then type a short line.\r\n"
        "Key: ";
    static char line_prompt[] = "\r\nLine: ";
    static char done[] = "\r\n";
    KBDKEYINFO key;
    STRINGINBUF in;
    char buffer[64];
    USHORT rc;
    unsigned i;

    key.chChar = 0;
    key.chScan = 0;
    key.fbStatus = 0;
    key.bNlsShift = 0;
    key.fsState = 0;
    key.time = 0;
    for (i = 0; i < sizeof(buffer); ++i)
        buffer[i] = 0;
    in.cb = (USHORT)(sizeof(buffer) - 1U);
    in.cchIn = 0;

    rc = KBDFLUSHBUFFER((USHORT)0);
    if (rc != 0)
        return (int)rc;

    rc = tty(intro, (USHORT)(sizeof(intro) - 1U));
    if (rc != 0)
        return (int)rc;

    rc = KBDCHARIN(&key, (USHORT)0, (USHORT)0);
    if (rc != 0)
        return (int)rc;

    rc = tty(line_prompt, (USHORT)(sizeof(line_prompt) - 1U));
    if (rc != 0)
        return (int)rc;

    rc = KBDSTRINGIN(buffer, &in, (USHORT)0, (USHORT)0);
    if (rc != 0)
        return (int)rc;

    (void)tty(done, (USHORT)(sizeof(done) - 1U));

    if (in.cchIn < sizeof(buffer))
        buffer[in.cchIn] = 0;
    else
        buffer[sizeof(buffer) - 1U] = 0;

    printf("key char=%u scan=%u status=%u state=%u time=%lu\r\n",
           (unsigned)key.chChar,
           (unsigned)key.chScan,
           (unsigned)key.fbStatus,
           (unsigned)key.fsState,
           (unsigned long)key.time);
    if (key.chChar >= 32U && key.chChar < 127U)
        printf("key character='%c'\r\n", key.chChar);
    printf("line cb=%u cchIn=%u text='%s'\r\n",
           (unsigned)in.cb, (unsigned)in.cchIn, buffer);

    return 0;
}
