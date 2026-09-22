/*
 * m29m-count.c - M29M pipeline line-count fixture.
 */
#include <stdio.h>

int main(void)
{
    int ch;
    unsigned long lines;
    int any;
    int last;

    lines = 0UL;
    any = 0;
    last = '\n';
    while ((ch = getchar()) != EOF) {
        any = 1;
        last = ch;
        if (ch == '\n')
            ++lines;
    }
    if (ferror(stdin))
        return 2;
    if (any && last != '\n')
        ++lines;
    printf("LINES=%lu\n", lines);
    return ferror(stdout) ? 3 : 0;
}
