# M31G R5 - native PMWP personality for NEKO ordinal 203

R4 proved `WinLoadMenu` and then reached NEKO's first PMWP import. Because
PMWP was not a compatibility personality, os2host32 attempted to execute the
real OS/2 2.0 GA Workplace Shell `PMWP.DLL` as a guest module.

That is intentionally **not** the R5 direction. The real shell DLL contains a
16-bit object, selector/far-pointer fixups, and dependencies on SOM, PMDRAG,
PMCTLS, SESMGR and other shell infrastructure. Those requirements are outside
the V1/native-Win32 branch.

## PMWP.203 research

The actual GA DLL proves ordinal 203 is a 32-bit export with a four-argument
cdecl-style stack frame and a zero-extended return in EAX. NEKO's single call
site also pushes four 32-bit arguments and takes EAX==0 as success. It occurs
immediately after `WinInitialize` and `WinCreateMsgQueue` and passes values
consistent with early shell/application registration.

A trustworthy historical public API name for ordinal 203 has not been found.
R5 therefore exports it honestly as:

    PMWPOrdinal203 @203 NONAME

The implementation accepts the advisory shell-registration/initialisation
request and returns full-EAX zero. It traces the four raw arguments under
`OS2_PM_TRACE` but does not dereference them or special-case NEKO.

## Build / test

**First rename the historical IBM `PMWP.DLL` if you copied it into the tree.**
For example:

    ren PMWP.DLL PMWP-GA.DLL

Then build the native personality:

    make PMWP.dll os2host32.exe
    make m31g-neko-r5-check
    os2host32.exe --run examples\m31g-neko\NEKO.EXE

or run:

    examples\m31g-neko\m31g-neko-r5-test.cmd

R5 success is that `PMWP.203` resolves from the native Win32 PMWP personality
and the old `guest DLL ... PMWP.dll needs unsupported fixup/mixed-mode
machinery` boundary disappears. Capture the next actual runtime boundary before
adding R6.
