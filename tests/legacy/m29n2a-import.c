/*
 * M29N2a recursive guest-DLL import regression.
 *
 * EXE -> M29N2A.DLL -> M29N2B.DLL
 */
#include <stdio.h>

extern unsigned long M29N2AImport(void);

int main(void)
{
    unsigned long value;

    value = M29N2AImport();
    printf("M29N2A_CHAIN_VALUE=%08lX\n", value);
    if (value != 0x29A20043UL) {
        puts("M29N2A_CHAIN_BAD");
        return 1;
    }
    puts("M29N2A_GUEST_DLL_CHAIN_OK");
    return 0;
}
