/*
 * M29K4 detached-process fixture.
 *
 * Waits briefly so the launching shell has time to regain its prompt, then
 * proves that a detached OS/2 child kept its current directory/environment
 * and can still use an explicitly redirected stdout handle.
 */
#define INCL_DOSPROCESS
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    FILE *f;
    const char *path;
    const char *token;

    path = argc > 1 ? argv[1] : "m29k4-detach.ok";
    token = getenv("M29K4_TOKEN");

    DosSleep(750UL);

    f = fopen(path, "wb");
    if (f == NULL)
        return 2;
    fputs("M29K4_DETACH_MARKER_OK\r\n", f);
    if (token != NULL) {
        fputs("TOKEN=", f);
        fputs(token, f);
        fputs("\r\n", f);
    }
    if (fclose(f) != 0)
        return 3;

    puts("M29K4_DETACH_STDOUT_OK");
    return 0;
}
