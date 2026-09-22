# Milestone 30G - EMX NLS.DLL bridge

M30G follows M30F after the EMX startup path reached unresolved import
`NLS.6`.

The exact EMX callsite uses NLS ordinal 6 as OS/2 2.x
`DosQueryDBCSEnv(12, &COUNTRYCODE, buffer)`.  It consumes the returned buffer
as pairs of inclusive DBCS lead-byte ranges.  For the current western/SBCS
host personality, M30G returns a zero-filled vector (no DBCS lead bytes).

The same EMX.DLL also imports NLS ordinal 7, `DosMapCase`, so M30G implements
that adjacent API now as well.  The first implementation maps ASCII a-z to
A-Z and leaves bytes >= 0x80 alone rather than incorrectly applying the
Windows ANSI code page to OS/2 OEM bytes.

Changes:

* Added `NLS.dll` compatibility DLL.
* NLS.6 = `DosQueryDBCSEnv`.
* NLS.7 = `DosMapCase`.
* Added NLS to the compatibility-module list.
* Removed the M30A blanket NLS trap deferral now that the imported NLS calls
  have real implementations.
* `make compat` now builds `NLS.dll`.

Useful tracing:

    set OS2_TRACE_MODULES=1
    set OS2_TRACE_NLS=1

Expected startup trace includes:

    loaded NLS.dll
    resolved NLS         .6 -> ...
    resolved NLS         .7 -> ...
    M30G NLS: DosQueryDBCSEnv cb=12 country=0 cp=0 -> SBCS empty vector

Then EMX should either continue toward the program's `hi` output or expose the
next concrete missing OS/2 API.
