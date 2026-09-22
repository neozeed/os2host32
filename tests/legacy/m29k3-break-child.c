/* M29K3 foreground Ctrl+C fixture. */
#define INCL_DOSPROCESS
#include <os2.h>
#include <stdio.h>

int main(void)
{
    puts("M29K3_BREAK_CHILD_WAITING (press Ctrl+C)");
    DosSleep(10000UL);
    puts("M29K3_BREAK_CHILD_FINISHED_WITHOUT_BREAK");
    return 23;
}
