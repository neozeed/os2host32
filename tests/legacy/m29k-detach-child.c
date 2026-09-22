/*
 * M29K detached child fixture.
 * Waits long enough for the parent prompt to return, then leaves a marker.
 */
#define INCL_DOSPROCESS
#include <os2.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    FILE *f;
    const char *path;

    path = argc > 1 ? argv[1] : "m29k-detach.ok";
    DosSleep(1200UL);
    f = fopen(path, "wb");
    if (f == NULL)
        return 2;
    fputs("M29K_DETACH_CHILD_OK\r\n", f);
    if (fclose(f) != 0)
        return 3;
    return 0;
}
