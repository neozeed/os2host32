/*
 * c386-far16-kbdflush-test.c - Microsoft C/386 scalar-only far16 probe.
 *
 * This deliberately has no far pointer arguments.  It proves that the M29E
 * descriptor-driven bridge can handle the compiler's 32->16 helper when the
 * Pascal frame contains only one USHORT argument.
 */

#include <stdio.h>

typedef unsigned short USHORT;

USHORT _far16 _pascal KBDFLUSHBUFFER(USHORT hkbd);

int main(void)
{
    USHORT rc;

    rc = KBDFLUSHBUFFER((USHORT)0);
    printf("KbdFlushBuffer rc=%u\r\n", (unsigned)rc);
    return (int)rc;
}
