/* Simple Microsoft C/386 / OS/2 DosBeep regression test. */
#define INCL_DOS
#include <os2.h>

int main(void)
{
    DosBeep(440, 500);
    DosBeep(660, 500);
    DosBeep(880, 1000);
    return 0;
}
