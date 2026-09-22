# M31G R2 - NEKO reaches PMSHAPI

R1 proved that the OS/2 2.0 GA `NEKO.EXE` is a clean V1/native-Win32 target
and added generic LX iterated-page decoding.  On Win32 the first import
boundary after successful mapping is `PMSHAPI.129`.

Historical OS/2 API/ordinal material maps ordinal 129 to:

    WinRemoveSwitchEntry(HSWITCH hswitch)

R2 adds exactly that generic shell API.  The existing PMSHAPI personality
already returns a synthetic switch handle from `WinAddSwitchEntry`; the real
Win32 top-level window is already present in the host task switcher.  R2
therefore accepts removal of the compatibility switch entry and returns 0
(success), with optional `OS2_PM_TRACE` diagnostics.

## Build / test

    make PMSHAPI.dll os2host32.exe
    make m31g-neko-r2-check
    os2host32.exe --run examples\m31g-neko\NEKO.EXE

or run:

    examples\m31g-neko\m31g-neko-r2-test.cmd

The expected R2 result is simply to move beyond `PMSHAPI ordinal 129 is not
exported`.  Capture the next boundary rather than implementing the remaining
NEKO imports speculatively.
