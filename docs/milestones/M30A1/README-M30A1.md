# Milestone 30A1 — EMX first-run diagnostic checkpoint

M30A1 keeps the M30A bridge code unchanged and adds a reproducible import-surface
audit plus a Windows test driver.  The supplied EMX DLL has 108 distinct ordinal
imports: 64 are currently exported by the compatibility DLL set and 44 are left
as M30 trap-on-use imports.  A deferred import is not assumed to be required by
Dhrystone; the first one actually reached is the useful next implementation target.

The mixed-mode boundary remains contained: the nine generic far16 wrappers are
redirected through the native EMX dispatcher, and the tiny 147-byte 16-bit startup
object is not executed by Win32.

## Test

Put `emx.dll` and `dhyrstone.exe` beside the built M30A1 files, then run:

    m30a1-emx-test.cmd

The script first scans EMX, then launches Dhrystone.  Preserve the complete output
if it reaches an `M30 deferred import reached` diagnostic; that module/ordinal is
the next API to add.
