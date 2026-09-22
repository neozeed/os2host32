/*
 * pmwp.c - native PMWP compatibility personality.
 *
 * M31G R5 originally modelled OS/2 2.0 GA PMWP ordinal 203 as an advisory
 * success call because its historical public name was not known.  R14 adds
 * stronger evidence from the original GA PMWP.DLL: the successful path calls
 * DOSCALLS.318 (DosLoadModule) with argument 3 as the module name and argument
 * 2 as the HMODULE output pointer.  NEKO passes "NEKO" and a writable global;
 * the returned HMODULE is then used for its RT_POINTER animation resources.
 *
 * Keep the export ordinal-only -- the historical public symbolic name still
 * has not been established -- but reproduce the proven loader behaviour.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#ifndef __cdecl
#define __cdecl
#endif

#define O2_ERROR_INVALID_FUNCTION   1UL
#define O2_ERROR_INVALID_PARAMETER 87UL

typedef DWORD (__cdecl *PFNDOSLOADMODULE)(char *, DWORD,
                                           const char *, DWORD *);

static int pmwp_trace_enabled(void)
{
    char value[8];
    DWORD n;
    n = GetEnvironmentVariableA("OS2_PM_TRACE", value, sizeof(value));
    return n != 0 && n < sizeof(value) && value[0] != '0';
}

/*
 * PMWP ordinal 203 (OS/2 2.0 GA): historical public name unknown.
 * Proven ABI: four 32-bit cdecl arguments.  The GA successful path is
 * structurally:
 *
 *   DosLoadModule(fail_name, 0x105, (PCSZ)arg3, (PHMODULE)arg2)
 *
 * Argument 4 participates in the native PMWP failure/reporting path; the V1
 * personality returns the DosLoadModule APIRET directly and leaves UI error
 * reporting to the caller/host.
 */
DWORD __cdecl PMWPOrdinal203(DWORD arg1, DWORD arg2, DWORD arg3, DWORD arg4)
{
    HMODULE dos;
    PFNDOSLOADMODULE load;
    char fail_name[0x105];
    DWORD rc;
    int borrowed;

    (void)arg1;
    (void)arg4;
    if (!arg2 || !arg3)
        return O2_ERROR_INVALID_PARAMETER;

    borrowed = 1;
    dos = GetModuleHandleA("DOSCALLS.dll");
    if (!dos) {
        borrowed = 0;
        dos = LoadLibraryA("DOSCALLS.dll");
    }
    if (!dos)
        return O2_ERROR_INVALID_FUNCTION;

    load = (PFNDOSLOADMODULE)GetProcAddress(dos, MAKEINTRESOURCEA(318));
    if (!load) {
        if (!borrowed)
            FreeLibrary(dos);
        return O2_ERROR_INVALID_FUNCTION;
    }

    fail_name[0] = 0;
    rc = load(fail_name, (DWORD)sizeof(fail_name),
              (const char *)(DWORD)arg3, (DWORD *)(DWORD)arg2);

    if (pmwp_trace_enabled()) {
        DWORD hmod;
        hmod = *(DWORD *)(DWORD)arg2;
        fprintf(stderr,
                "PMWP: ordinal 203 module=\"%s\" rc=%lu hmod=%08lX fail=\"%s\"\n",
                (const char *)(DWORD)arg3, (unsigned long)rc,
                (unsigned long)hmod, fail_name);
        fflush(stderr);
    }

    if (!borrowed)
        FreeLibrary(dos);
    return rc;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance; (void)reason; (void)reserved;
    return TRUE;
}
