/* Microsoft C/386 Beta2 SDK declarations; no handwritten OS/2 API prototypes. */
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSQUEUES
#define INCL_DOSFILEMGR
#define INCL_DOSMISC
#include <os2.h>
#include <stdio.h>
#include <string.h>
#define FOREVER 0xffffffffUL
static void r8_fail(ULONG line)
{
    printf("%s FAIL line %lu\n",TEST_NAME,line);
    DosExit(EXIT_PROCESS,1);
}
#define CHECK(x) do { if (!(x)) r8_fail((ULONG)__LINE__); } while (0)
