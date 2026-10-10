# Soft386 jar memory R2

Soft386 DOSCALLS.348 (DosQuerySysInfo) reports the jar's memory budget on
Windows and non-Windows hosts. It is explicitly serviced locally before the
native DOSCALLS bridge, using the shared DOS core's validation and serialization.
The native Win32 DOSCALLS.dll continues to report host memory budgets.

This source snapshot includes the previous PMWIN.867 and VIO/viofetch R1 work.
This document supersedes R1's description of Soft386 using host memory figures.

## Query values

| Index | Value in Soft386 |
| --- | --- |
| 17, QSV_TOTPHYSMEM | Fixed 64 MiB guest RAM arena |
| 18, QSV_TOTRESMEM | 64 MiB minus allocator-usable free bytes; occupied/reserved/unavailable capacity, not measured host residency |
| 19, QSV_TOTAVAILMEM | Total allocator-usable free bytes |
| 20, QSV_MAXPRMEM | Largest block that one allocation can currently obtain |
| 21, QSV_MAXSHMEM | Same largest-block compatibility approximation; no separate shared-memory arena is introduced |

For a fresh jar, indices 17..21 are 64, 32, 32, 32, 32 MiB. Viofetch displays
index 19, so it should show 32.00 MB, or less if startup/library code has already
allocated memory. The arena does not grow automatically toward 4 GiB.

The allocation pool occupies 0x01000000..0x03000000. Reporting follows the
existing allocator: freed blocks are reused whole, are not split or coalesced,
and retain their original size when reused for a smaller request. The untouched
pool tail counts only while a virgin entry remains in the 256-entry allocation
table. Thus total free bytes can exceed the largest available block, and table
exhaustion can make untouched bytes unavailable. Allocation accounting is page
rounded. Protection changes do not release backing memory.

The allocator itself, native DOS DLL, NLS/MSG routing, PM rendering and WHP are
unchanged by this follow-up. No additional DOS export or guest import is needed.

## Patch selection

- SOFT386-JAR-MEMORY-R2-from-R1.patch: apply after PMWIN867-VIOFETCH R1.
- OS2HOST32-PMWIN867-VIOFETCH-JAR-R2-from-upload.patch: cumulative patch against
  the original os2host32-main.zip supplied in this conversation.

Apply exactly one suitable patch at the repository root with `git apply`.
The full source archive already contains all changes; do not patch it again.
Neither patch contains executable binaries.

## Build and install

From soft386 in a MinGW/i386 shell:

    make win32 WINCC=i686-w64-mingw32-gcc

On a cross-build host, the same command applies. `make CC=i686-w64-mingw32-gcc`
also supports a native MinGW shell (the default target name lacks .exe).
For a standalone binary without a libgcc DLL dependency:

    make win32 WINCC=i686-w64-mingw32-gcc CFLAGS="-O2 -std=gnu99 -Wall -Wextra -Wno-unused-function -Wno-misleading-indentation -static-libgcc"

Replace the soft386_os2.exe selected by your os2host32 loader (normally beside
os2host32.exe). Replacing DOSCALLS.dll alone does not update this jar-local code.
No additional DLL replacement is required if R1's DLLs are already installed.
The supplied Win32 zip contains the rebuilt Soft386 executable, a real LE memory
fixture, checksums, and RUN-JAR-MEMORY.cmd; run the script before using viofetch.
It expects: `soft386: guest LE jar memory budget PASS` and exit status zero.

## Validation

- `make -C soft386 check-jar-memory`: PASS. Actual allocator/dispatch tests cover
  allocation/free, page rounding, whole-block reuse, fragmentation, exhaustion,
  protection changes, malformed output buffers and invalid query ranges. A loaded
  hostile native provider verifies that ordinal 348 never reaches that provider.
- Real LE fixture executed by Tiny386: PASS; checks all five fields before
  allocation, after a 4 MiB allocation, and after free.
- Shared DOS personality core and catalogue check: PASS.
- Existing Soft386 R1 and R2 runtime/bridge regression scripts: PASS.
- i686 MinGW build: PASS. Executable imports only Windows system DLLs and has no
  GlobalMemoryStatusEx import or libgcc DLL dependency.
- Both patches apply cleanly to their documented baseline and reproduce the
  packaged source byte for byte.

Linux host runtime tests and Windows cross-build validation were performed here.
Live Windows execution of this R2 binary remains to be checked on your machine.
The previously documented frozen DOSCALLS/SESMGR hash-check failures in the
uploaded baseline are not changed or bypassed.
