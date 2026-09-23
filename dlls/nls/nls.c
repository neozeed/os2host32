/*
 * NLS.DLL - OS/2 ordinal veneer for the shared personality NLS service.
 *
 * DOSCALLS owns the process NLS state.  Keeping these entry points as a thin
 * veneer ensures NLS.5/.6/.7 observe DosSetProcessCp (DOSCALLS.289) while the
 * same common service can also be embedded directly by WHP/software backends.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef __cdecl
#define __cdecl
#endif

typedef unsigned long O2ULONG;
typedef unsigned long O2APIRET;

__declspec(dllimport) O2APIRET __cdecl O2NlsQueryCtryInfo(
    O2ULONG cb, const void *countrycode, void *countryinfo, O2ULONG *actual);
__declspec(dllimport) O2APIRET __cdecl O2NlsQueryDBCSEnv(
    O2ULONG cb, const void *countrycode, void *buffer);
__declspec(dllimport) O2APIRET __cdecl O2NlsMapCase(
    O2ULONG cb, const void *countrycode, void *buffer);

O2APIRET __cdecl DosQueryCtryInfo(O2ULONG cb, const void *countrycode,
                                  void *countryinfo, O2ULONG *actual)
{
    return O2NlsQueryCtryInfo(cb, countrycode, countryinfo, actual);
}

O2APIRET __cdecl DosQueryDBCSEnv(O2ULONG cb, const void *countrycode,
                                 void *buffer)
{
    return O2NlsQueryDBCSEnv(cb, countrycode, buffer);
}

O2APIRET __cdecl DosMapCase(O2ULONG cb, const void *countrycode, void *buffer)
{
    return O2NlsMapCase(cb, countrycode, buffer);
}
