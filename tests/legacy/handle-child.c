/*
 * handle-child.c - M28C genuine LX child used to prove inherited stdout.
 *
 * This intentionally uses the OS/2 DosWrite API directly rather than the
 * host CRT so the pipe/standard-handle path is unambiguous.
 */
#define INCL_DOSFILEMGR
#include <os2.h>

int main(void)
{
    static const char msg[] = "M28C_PIPE_CHILD_OK\r\n";
    ULONG actual;
    APIRET rc;

    actual = 0;
    rc = DosWrite((HFILE)1, (PVOID)msg,
                  (ULONG)(sizeof(msg) - 1U), &actual);
    if (rc != 0)
        return 2;
    return actual == (ULONG)(sizeof(msg) - 1U) ? 0 : 3;
}
