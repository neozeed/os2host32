WHP OS/2 V2 R4 - phase 1 LX loading
==================================
Build from an x64 Visual Studio Developer Command Prompt:
    build-v2.cmd

Offline check (does not query WHP or run guest code):
    whp_os2_v2_hi.exe --check hi.exe
    whp_os2_v2_hi.exe --check hi-lx.exe
    whp_os2_v2_hi.exe --check C:\2\sar\sarienlx.exe

Expected Sarien summary:
    LX loader check: 85 pages, 2 objects
    fixups: 1323 internal records / 2709 sites; 82 external sites
    PASS: page mapping and fixup application; 54 ordinal + 0 named imports.

--check uses temporary diagnostic addresses for ALL imports, applies real
relocations, reports CHECK ONLY, and frees the image. It does not resolve
exports, initialise DLLs, or establish that imported services are implemented.

Live regression order:
    whp_os2_v2_hi.exe hi.exe
    whp_os2_v2_hi.exe hi-lx.exe
    whp_os2_v2_hi.exe thread1.exe
    whp_os2_v2_hi.exe threadtort.exe 30
    whp_os2_v2_hi.exe sem1.exe
    whp_os2_v2_hi.exe semtort.exe 24
    whp_os2_v2_hi.exe dlltest.exe
    whp_os2_v2_hi.exe dllthread.exe 30

Use existing compiled R3 regressions or the included C/386 build scripts.
hi-lx.exe is a DERIVED test fixture: hi.exe with an LX header/page map and
fixup records reordered from LE physical-page order into LX logical-page
order. Code/data bytes and preferred object addresses are unchanged. Offline
mapped and relocated 64 MiB images match hi.exe byte for byte. Expected guest
output and exit status are the same as hi.exe (the documented fixture rc=4).
It is not a historical original or a fresh compiler build.

Changes
-------
* Accept both LE and LX signatures in the common EXE/DLL loader.
* LX uses 8-byte map entries, shifted data offsets, and per-page byte counts.
* LX fixup ownership follows logical module-page order, including zero pages.
* Normal short pages retain zero-filled tails; explicit zero pages remain zero.
* Reject unsupported iterated/compressed/range page encodings explicitly.
* Check shift, page size/count/range, file data bounds and object map bounds.
* Check main entry/stack bounds and overlapping main objects.
* Move startup area from 00080000..0009FFFF to 00E00000..00E1FFFF:
  Sarien's data/stack occupies 00030000..0008FCAF and overlapped the old area.
* Reject main objects overlapping the GDT, veneers, startup area, or the
  allocator/DLL region at or above 01000000.
* Preserve R3 guest DLLs, cooperative threads, event semaphores, and the
  compiler-warning cleanup (OS2_ status constants, noreturn fatal).
* Normal execution explicitly rejects PMWIN/PMGPI/QUECALLS imports with a
  phase-1 diagnostic. Do not copy V1 Win32 personality DLLs into V2.

Validation performed here
-------------------------
Exact loader functions extracted from the delivered C source compiled with
GCC -std=c11 -Wall -Wextra -Werror and address/undefined-behaviour sanitizers.
Leak sanitizer disabled because this execution environment does not support it.
Sarien's normal/zero page mapping and all 2791 relocation sites matched an
independent Python decoder byte for byte across the entire 64 MiB RAM image.
Both hi formats passed and produced identical pre/post-relocation images.
Malformed LX shift, data offset, oversized page, unsupported page encoding,
and object map index were rejected cleanly.
These are offline loader checks, NOT a full Windows build or live WHP run.
MSVC /W4 build and live LE/LX/thread/DLL regressions await Windows testing.

Next boundary
-------------
Sarien is not runnable yet. Its PMWIN, PMGPI, QUECALLS and remaining DOSCALLS
services are not implemented in V2. TIB/PIB, callbacks, dynamic DLL lifecycle,
pre-emption and FPU context preservation remain separate work.
