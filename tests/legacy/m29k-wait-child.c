/* M29K asynchronous DosWaitChild fixture. */
#define INCL_DOSPROCESS
#include <os2.h>

int main(void)
{
    DosSleep(1000UL);
    return 23;
}
