# M30M2 - OS/2 PIB command-line bridge

M30M2 keeps the M30M1 sidecarless EMX relocation inference unchanged and fixes
the next compatibility bug exposed by `infocom.exe`.

## What the Infocom crash proved

Running:

    os2host32.exe --run infocom.exe nosuch.dat

still reached Infocom's `argc == 1` no-argument path.  The loader's process-entry
startup area already contained the requested guest tail, but EMX obtains argc/argv
through `DosGetInfoBlocks()` and `PIB.pib_pchcmd`.

The compatibility DOSCALLS implementation incorrectly populated `pib_pchcmd` from
Win32 `GetCommandLineA()`.  That produced one ordinary C string describing the host:

    os2host32.exe --run infocom.exe nosuch.dat\0

OS/2 instead exposes the command area as two adjacent strings:

    infocom.exe\0nosuch.dat\0\0

EMX therefore saw one argument regardless of the guest tail.

## Fix

`os2host32.exe` now exports `OS2HostQueryMainCommand`, publishing the exact OS/2
command block already constructed for guest startup.  `DOSCALLS.DosGetInfoBlocks`
uses that bridge for `PIB.pib_pchcmd` and emits this diagnostic when
`OS2_TRACE_EMX_SELF=1`:

    M30M2 PIB CMD: argv0="infocom.exe" tail="nosuch.dat" bytes=...

The M30M test CMD now forwards all guest arguments with `%*`.

## Test

    make clean
    make tools compat
    m30m-emx-infer-run.cmd infocom.exe nosuch.dat

The first proof to look for is:

    M30M2 PIB CMD: argv0="infocom.exe" tail="nosuch.dat" ...

With a deliberately missing file, the desired next behaviour is the application's
normal failed-open/usage path rather than the former access violation at 01000A58.

Relocation recovery remains the same M30M1 experimental classifier.
