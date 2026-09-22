/*
 * c386-far16-kbd-test.c - Microsoft C/386 _far16 KbdCharIn bridge probe.
 *
 * Beta 2's 32-bit headers do not expose KBDKEYINFO/KbdCharIn, so define the
 * historical 16-bit ABI explicitly.  Microsoft C/386 6.00.081 then emits the
 * same 32->16 migration helper family that M29B proved for VioWrtTTY.
 */

#include <stdio.h>

typedef unsigned char  UCHAR;
typedef unsigned short USHORT;
typedef unsigned long  ULONG;

/* OS/2's KBDKEYINFO wire layout is packed on a two-byte boundary: 10 bytes. */
#pragma pack(2)
typedef struct _KBDKEYINFO {
    UCHAR  chChar;
    UCHAR  chScan;
    UCHAR  fbStatus;
    UCHAR  bNlsShift;
    USHORT fsState;
    ULONG  time;
} KBDKEYINFO;

typedef char KBDKEYINFO_must_be_10_bytes[(sizeof(KBDKEYINFO) == 10) ? 1 : -1];

USHORT _far16 _pascal KBDCHARIN(KBDKEYINFO _far16 *info,
                                USHORT wait,
                                USHORT hkbd);

int main(void)
{
    KBDKEYINFO key;
    USHORT rc;

    key.chChar = 0;
    key.chScan = 0;
    key.fbStatus = 0;
    key.bNlsShift = 0;
    key.fsState = 0;
    key.time = 0;

    printf("Press one key through C/386 far16 KbdCharIn: ");
    fflush(stdout);

    rc = KBDCHARIN(&key, (USHORT)0, (USHORT)0);

    printf("\r\nKbdCharIn rc=%u char=%u scan=%u status=%u state=%u time=%lu\r\n",
           (unsigned)rc,
           (unsigned)key.chChar,
           (unsigned)key.chScan,
           (unsigned)key.fbStatus,
           (unsigned)key.fsState,
           (unsigned long)key.time);

    if (key.chChar >= 32U && key.chChar < 127U)
        printf("character='%c'\r\n", key.chChar);

    return (int)rc;
}
