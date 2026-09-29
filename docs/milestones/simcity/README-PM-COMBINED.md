# Combined PMWIN + PMGPI source - 2026-09-29

One matched source snapshot from the Micropolis port thread. Includes the
complete pmwin.c/pmwin.def, pmgpi.c/pmgpi.def, and shared pmcompat.h.

Included fixes:

- WinStartTimer, ordinal 884, and the supplied WinStopTimer implementation,
  ordinal 885 NONAME, which was missing from the preceding archive.
- Multiple deferred top-level window shows.
- Bitmap palette application when creating/selecting DIBs.
- WinFillRect uses selected-bitmap bounds for memory presentation spaces.
- GpiBox uses native drawing for all bitmap depths, including 32-bit fills
  and outlines (toolbar selection and white map cursor rectangle).
- Existing resource-menu/accelerator support and tile-blit trace throttling.

## Build and install

This is a source archive. No prebuilt DLLs are included: the 32-bit MinGW
compiler and Windows runtime were not available in the editing environment.

Extract into an empty directory and build both DLLs together:

```
make -B -f Makefile.PM
```

If your 32-bit MinGW compiler is named gcc, use:

```
make -B -f Makefile.PM MINGW=gcc
```

Use a 32-bit compiler, not x86_64 MinGW. Close running host/guest processes,
then copy BOTH new PMWIN.dll and PMGPI.dll beside os2host32.exe. Remove or
replace stale copies in the guest working directory too, if present. No
Micropolis recompile or resource-binding step is needed for these DLL fixes.

For merging into an existing source tree, overlay the five files under dlls/;
Makefile.PM is optional and does not replace the project's main Makefile.
If that tree has newer changes from another branch, merge instead of blindly
overwriting it. SHA256SUMS.txt identifies all files in this snapshot.

## Checks

The included `python3 tests/pm/rectangle-check.py` regression passes against
the actual rectangle functions with mocked native drawing calls. The timer
definition and ordinal-885 export were checked for uniqueness and agreement.
This is not a complete DLL build or live Windows runtime validation.
