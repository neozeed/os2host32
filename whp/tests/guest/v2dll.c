/*
 * v2dll.c - CRT-free 32-bit C/386 OS/2 DLL for WHP V2.
 *
 * Tests:
 *   - ordinary executable -> guest DLL calls
 *   - DLL writable process-instance data
 *   - DLL -> DOSCALLS import (DosWrite)
 *   - one mapped DLL instance shared by all guest threads
 */

typedef unsigned long ULONG;
typedef void *PVOID;

extern ULONG DosWrite(ULONG hfile, PVOID buffer, ULONG count, ULONG *written);

static ULONG g_call_count;

ULONG V2DllAdd(ULONG a, ULONG b)
{
    return a + b;
}

ULONG V2DllNext(void)
{
    ++g_call_count;
    return g_call_count;
}

ULONG V2DllSay(char *s, ULONG len)
{
    ULONG written;

    written = 0;
    return DosWrite(1, (PVOID)s, len, &written);
}
