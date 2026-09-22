WHP OS/2 V2 R6 - C/386 directory search
======================================
Based on live-tested R5b file I/O, LE/LX loading, threads, events and guest DLLs.

Build host from x64 Visual Studio Developer Command Prompt:
    build-v2.cmd
Keep v2_fileio.h and v2_find.h beside whp_os2_v2_hi.c.
Expected banner: WHP OS/2 V2 R6 directory search.

Build guest from your recovered C/386 environment:
    build-c386-find.cmd
The build uses -B1 C1_386 and the supplied SDK os2.h declarations. No handwritten
API declarations and no extra DosOpen argument. C386LIB defaults to \c386\lib.

Run from the release directory with host and guest EXEs present:
    run-find1.cmd
Expected stdout: find1 PASS
Trace: find1-trace.txt in the release directory.

The runner refuses an existing v2-find-test directory. It creates that directory
and SUBDIR inside it. The guest exclusively creates ALPHA.TXT (3 bytes),
BETA.TXT (6 bytes), and HIDDEN.TXT (hidden, 1 byte). Successful runs remove the
files and directories. Failed runs retain the fixture for inspection; remove
it manually only after examining the error. Never run find1 in your game folder.

New DOSCALLS
------------
263 DosFindClose
264 DosFindFirst (the directory import needed by the supplied Sarien executable)
265 DosFindNext

FIL_STANDARD (level 1) uses the supplied C/386 Beta-2 FILEFINDBUF layout:
  dates/times 0..11, size at 12, allocation size at 16,
  USHORT attributes at 20, name length at 22, filename at 23.
A complete record needs 279 bytes, including the 256-byte filename field.
No oNextEntryOffset and no heuristic switch to GA FILEFINDBUF3 by buffer size.
Only this historical layout/information level is supported in this release.

One record is returned per successful call, even when the caller requests more.
The returned count is 1, or 0 at exhaustion/error; callers must honour it.
True multi-record packing is deferred. The count>1 regression tests partial
results across repeated calls, not a full multi-record batch.

Normal searches include ordinary/read-only/archive files. Hidden/system/
directory entries require their matching attribute flag. High-byte must-have
bits are also supported for the 0x37 attribute set. Wildcard matching and native
entry order are delegated to FindFirstFileA/FindNextFileA; OS/2-specific wildcard
edge cases, case-sensitive host directories and Unicode are not generalised.

32 live search slots. HDIR_CREATE allocates process-local IDs distinct from
file handles; IDs are not reused, so a stale dynamic handle stays invalid.
HDIR_SYSTEM=1 and restarting a currently valid search handle are supported.
The old search is replaced only after the new search has a valid first result.
A too-small FindNext buffer returns 111 without consuming the pending entry.
Exhaustion returns 18; close/double-close return 0/6. Searches close on host exit.
Names >255 bytes and files >4 GiB return 111; allocation sizes are approximated
by 4 KiB rounding, as in R5 file metadata. Other information levels and EAs are
not implemented.

Guest regression coverage
-------------------------
- SDK structure filename offset and capacity check
- exclusive fixture creation using the now-proven file APIs
- wildcard enumeration, names, lengths and file sizes
- two simultaneous independent search handles
- undersized FindNext buffer and successful retry without losing a match
- exhaustion and repeated exhaustion
- close, double-close and invalid/stale handle
- larger requested count and hidden-file inclusion
- no matches, unsupported information level and zero requested count
- HDIR_SYSTEM reuse and directory results
- fixture deletion after success

Offline validation
------------------
find1.c passes GCC -m32 -std=c89 -Wall -Wextra -Werror syntax checking against
the supplied C/386 OS/2 and C runtime headers, with legacy compiler keywords
mapped away and PM disabled for that check. This does not replace CL386 linking.
Exact v2_find.h and v2_fileio.h pass GCC warning-as-error compilation and address/
undefined-behaviour sanitizer execution. Leak sanitizer is disabled in this
environment. Tests use a deterministic mock Win32 directory backend and a
POSIX file backend; they are not live Windows API/WHP execution.
Directory tests include slot exhaustion and 64 close/reopen cycles in addition
to the guest cases. See tests/find-check.c and validation/find-check.out.
To repeat on Linux:
    gcc -std=c11 -Wall -Wextra -Werror tests/find-check.c -o /tmp/find-check
    /tmp/find-check
The inherited file test uses /tmp/v2-fileio-unit.dat as a disposable fixture.

Scope toward Sarien
-------------------
This wires Sarien's DOSCALLS.264 directory-search dependency. Sarien still cannot
run as a whole: PMWIN, PMGPI, QUECALLS, mutex, sleep and audio work remain.
Existing --check and the R5b stderr diagnostics remain available. Recheck
fileio1, the LE/LX hello programs, and the existing thread/event/DLL tests after
the first successful Windows directory run. Infocom save/restore is a useful
application regression; its unusual nonzero quit code remains unchanged.
