/*
 * c386-far16-vio-test.c - Microsoft C/386 _far16 VIO bridge probe.
 *
 * This deliberately avoids later OS/2 migration headers.  Microsoft C/386
 * 6.00.081 itself knows how to generate the 32->16 transition helper when a
 * function is declared _far16 _pascal.  LINK386 is given the missing imports
 * explicitly by c386-far16-vio-test.def.
 */

typedef unsigned short USHORT;

USHORT _far16 _pascal VIOWRTTTY(char _far16 *text,
                                USHORT count,
                                USHORT hvio);

int main(void)
{
    static char message[] = "hello from far16\r\n";
    return (int)VIOWRTTTY(message,
                          (USHORT)(sizeof(message) - 1U),
                          (USHORT)0);
}
