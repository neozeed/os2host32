/*
 * emit-test.c - simple OS/2 LX stdout pipe/redirection regression.
 */
#include <stdio.h>

int main(void)
{
    puts("one");
    puts("two");
    puts("three");
    return 0;
}
