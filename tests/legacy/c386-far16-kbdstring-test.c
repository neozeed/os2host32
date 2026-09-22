/*
 * c386-far16-kbdstring-test.c - Microsoft C/386 _far16 KbdStringIn probe.
 *
 * This deliberately uses two far16 pointers in one call: the destination
 * character buffer and the input/output STRINGINBUF length structure.
 */

#include <stdio.h>

typedef unsigned short USHORT;

#pragma pack(2)
typedef struct _STRINGINBUF {
    USHORT cb;
    USHORT cchIn;
} STRINGINBUF;
#pragma pack()

typedef char STRINGINBUF_must_be_4_bytes[(sizeof(STRINGINBUF) == 4) ? 1 : -1];

USHORT _far16 _pascal KBDSTRINGIN(char _far16 *buffer,
                                  STRINGINBUF _far16 *length,
                                  USHORT wait,
                                  USHORT hkbd);

int main(void)
{
    char buffer[64];
    STRINGINBUF length;
    USHORT rc;
    unsigned i;

    for (i = 0; i < sizeof(buffer); ++i)
        buffer[i] = 0;

    length.cb = (USHORT)(sizeof(buffer) - 1);
    length.cchIn = 0;

    printf("Type a short line through C/386 far16 KbdStringIn: ");
    fflush(stdout);

    rc = KBDSTRINGIN(buffer, &length, (USHORT)0, (USHORT)0);

    if (length.cchIn < sizeof(buffer))
        buffer[length.cchIn] = 0;
    else
        buffer[sizeof(buffer) - 1] = 0;

    printf("KbdStringIn rc=%u cb=%u cchIn=%u text='%s'\r\n",
           (unsigned)rc,
           (unsigned)length.cb,
           (unsigned)length.cchIn,
           buffer);

    return (int)rc;
}
