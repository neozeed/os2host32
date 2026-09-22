# M31E R3 - prerelease INITINSTANCE DLL lifecycle ABI

OPENDLG.DLL exposed a lifecycle ABI older than the DllInitTerm(hmod,flag)
contract proven by the synthetic M29N2b.2 fixtures.

The historical OPENDLG module is marked INITINSTANCE but has no TERM flag.  Its
actual entry point is the SDK sample's `InitLibrary(hPDLL, hmod)` routine, which
stores the second argument as its HMODULE before loading DLL-owned strings.

R3 therefore selects the legacy startup ABI for INITINSTANCE-only guest DLLs:

    arg1 = 0          (hPDLL; OPENDLG does not use it)
    arg2 = guest HMODULE

Modules carrying the M29N2-style TERM flag retain DllInitTerm(hmod,flag).
Termination now also honors the LE TERM flag, so an INITINSTANCE-only entry is
not incorrectly called a second time at process shutdown.

Expected OPENDLG trace:

    GUESTMOD INITCALL: OPENDLG ... abi=legacy-initinstance
    GUESTMOD INITRET:  OPENDLG ... result=00000001
    GUESTMOD READY:    OPENDLG ...

This is a loader-level compatibility rule, not an OPENDLG name special case.
