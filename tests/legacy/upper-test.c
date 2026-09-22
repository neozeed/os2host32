/*
 * upper-test.c - simple OS/2 LX stdin/stdout pipe regression.
 * Compile with the recovered Microsoft C/386 toolchain.
 */
#include <stdio.h>
#include <ctype.h>

int main(void)
{
    int ch;

    while ((ch = getchar()) != EOF) {
        if (putchar(toupper((unsigned char)ch)) == EOF)
            return 2;
    }
    if (ferror(stdin) || ferror(stdout))
        return 1;
    return 0;
}
