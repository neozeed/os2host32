/* M29L1 START/session fixture: run in a new SESMGR-created session. */
#define INCL_DOSPROCESS
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    const char *path;
    const char *token;
    FILE *f;

    path = argc > 1 ? argv[1] : "m29l-start.ok";
    DosSleep(700UL);
    f = fopen(path, "wb");
    if (f == NULL)
        return 2;
    fputs("M29L_START_SESSION_OK\r\n", f);
    token = getenv("M29L_TOKEN");
    if (token != NULL) {
        fputs("TOKEN=", f);
        fputs(token, f);
        fputs("\r\n", f);
    } else {
        fputs("TOKEN=<unset>\r\n", f);
    }
    if (fclose(f) != 0)
        return 3;
    return 17;
}
