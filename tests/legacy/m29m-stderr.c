/*
 * m29m-stderr.c - M29M stdout/stderr redirection fixture.
 */
#include <stdio.h>

int main(void)
{
    fputs("STDOUT_LINE\n", stdout);
    fflush(stdout);
    fputs("STDERR_LINE\n", stderr);
    fflush(stderr);
    return 0;
}
