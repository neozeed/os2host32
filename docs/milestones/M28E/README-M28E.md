# M28E - one host for old LE and newer LX C/386 binaries

`os2host32.exe` now auto-detects both 32-bit OS/2 linear executable formats.
There is no command-line switch to choose the format:

```cmd
os2host32.exe --scan program.exe
os2host32.exe --run program.exe
```

An old LINK386 LE program should report `LE header`, while the newer DDK linker
output reports `LX header`.  `DosExecPgm` does not need to know which format a
child uses: both are routed to the same `os2host32 --run-quiet` entry point and
the loader chooses the parser itself.

The direct execution gate is intentionally the same narrow subset for both
formats: flat 32-bit objects, internal `OFF32` fixups and external ordinal
`REL32` imports.  Mixed 16/32-bit images can still be inventoried with
`--scan`, but need selector/alias machinery before they can execute directly.

For LE specifically, the loader handles the important format difference that
`e32_mpages` is a count of physical file-backed pages, while the four-byte LE
object page map can contain additional logical ZEROED pages.  Fixup ownership
therefore follows the page map's 24-bit physical page number rather than
assuming object-page number == physical-page number.

## Early Beta 2 headers

`cmdos2_os2.c` no longer requires the SDK typedef names `FILEFINDBUF3` or
`FILESTATUS3`.  It uses private FIL_STANDARD layouts instead.  This is meant
to let the source compile with early prerelease headers that predate those
names; the real Beta 2 build/run is still the ABI test.

A useful first LE test is a tiny program linked with the original linker, then:

```cmd
os2host32.exe --scan old-le-test.exe
os2host32.exe --run old-le-test.exe
```

If it reaches the guest, the output should explicitly say `LE header` and
`Transfer : original LE entry ...`.
