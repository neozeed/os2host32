/* Microsoft C/386, OS/2 SDK declarations supply the calling convention. */
#define INCL_DOSPROCESS
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv)
{
    char *value;
    if (argc > 1 && !strcmp(argv[1], "check")) {
        value = getenv("WHP_EXEC_TEST");
        if (argc != 3 || strcmp(argv[2], "two words") ||
            !value || strcmp(value, "child")) return 91;
        puts("execchild arguments/environment PASS");
        return 37;
    }
    if (argc > 1 && !strcmp(argv[1], "slow")) {
        DosSleep(500);
        return 23;
    }
    if (argc > 1 && !strcmp(argv[1], "capture")) {
        puts("CHILD_STDOUT_OK");
        return 0;
    }
    puts("execchild running inside WHP");
    return argc > 1 ? atoi(argv[1]) : 0;
}
