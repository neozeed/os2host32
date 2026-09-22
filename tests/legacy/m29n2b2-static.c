/* M29N2b.2 load-time lifecycle/process-exit regression. */
#include <stdio.h>

extern unsigned long M29N2CValue(void);

int main(void)
{
    unsigned long v;
    v = M29N2CValue();
    printf("M29N2B2_STATIC_VALUE=%08lX\n", v);
    if (v != 0x29B20043UL)
        return 1;
    puts("M29N2B2_PROCESS_EXIT_BODY_OK");
    return 0;
}
