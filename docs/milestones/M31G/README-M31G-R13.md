# M31G R13 - NEKO import-complete tranche

R13 is the deliberate controlled "hail-mary" after R12: instead of spending
another context window walking one ordinal at a time, it implements the entire
remaining *known* import surface of the frozen untouched OS/2 2.0 GA NEKO.EXE.
Every addition is still generic OS/2 compatibility behavior; there are no
NEKO/e.exe name checks.

NEKO's remaining 13 imports are now covered:

- HELPMGR.51 `WinCreateHelpInstance`
- HELPMGR.54 `WinAssociateHelpInstance`
- PMGPI.399 `GpiLoadBitmap`
- PMSHAPI.117 `PrfQueryProfileData`
- PMSHAPI.118 `PrfWriteProfileData`
- PMWIN.702 `WinBeginEnumWindows`
- PMWIN.730 `WinDrawBitmap`
- PMWIN.737 `WinEndEnumWindows`
- PMWIN.756 `WinGetNextWindow`
- PMWIN.775 `WinIsWindowVisible`
- PMWIN.822 `WinQueryPointerInfo`
- PMWIN.823 `WinQueryPointerPos`
- PMWIN.872 `WinSetSysModalWindow`

The separate e.exe path also gains DOSCALLS.212 `DosError`.

Notable implementation details:

- PM window enumeration snapshots immediate children in Z order and exposes an
  opaque HENUM lifetime.
- WinDrawBitmap draws the existing shared CompatBitmap representation into a
  PM HPS with OS/2 bottom-left coordinate conversion.
- Pointer info/position map native cursor state back into OS/2 structures and
  coordinates.
- `GpiLoadBitmap` queries the guest RT_BITMAP registry exported by PMWIN,
  decodes OS/2 1.x/2.x bitmap headers (including BITMAPARRAY wrapping), carries
  palettes into the DIB section, and supports requested scaling.
- Binary profile data is persisted as explicit `@HEX:` payloads in the existing
  HINI/path bridge so arbitrary bytes survive host INI storage.
- HELPMGR remains a deliberately small native personality: create/associate/
  destroy preserve help-instance lifetime without pretending to implement the
  complete IBM IPF engine.
- DosError validates/stores the documented low two policy bits and rejects
  unsupported flags with ERROR_INVALID_PARAMETER.

`tools/check_m31g_neko.py` now reports **0 imports outside current .def
coverage** for the frozen NEKO specimen.  R13 therefore moves the experiment
from loader/import completion to runtime semantics.  The most likely next
behavioral boundary is NEKO's use of `WinSubclassWindow` on its native STATIC
cat window combined with timer delivery; that remains intentionally separate
from this import-completion tranche.
