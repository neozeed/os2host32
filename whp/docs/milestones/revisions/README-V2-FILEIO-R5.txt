WHP OS/2 V2 R5 - Infocom file-I/O slice
=====================================
Built on the live-tested R4 LE/LX/thread/event/DLL baseline.

Build host (x64 Visual Studio Developer Command Prompt):
    build-v2.cmd
Keep v2_fileio.h beside whp_os2_v2_hi.c. The build command is unchanged.

Build guest regression (C/386 environment):
    build-c386-fileio.cmd
    whp_os2_v2_hi.exe fileio1.exe
Expected: fileio1 PASS, process exit 0.
The test exclusively creates V2IO-TST.DAT in the current directory and removes
it on success. If it already exists the test fails without replacing it.
Inspect a leftover after a failure before removing it to retry.

Infocom:
    whp_os2_v2_hi.exe C:\OS2\demos\infocom\infocom.exe C:\path\to\story.dat
Replace the story path with your existing game file. Its embedded usage is:
    Usage: infocom [-aehoprtv] <filename>
Absolute paths avoid confusion: relative paths use the host's current directory,
not the EXE directory. Saves/transcripts use ordinary Windows files in that
context. First test opening the game, entering commands, then save/restore.
Capture the full console trace if an API or guest execution fails.

Implemented in this slice
-------------------------
223 DosQueryPathInfo: FIL_STANDARD (1), full path (5)
230 DosGetDateTime: byte-packed 12-byte guest DATETIME
257 DosClose
259 DosDelete
272 DosSetFileSize: preserves file position
273 DosOpen: eight-argument SDK ABI; tolerates legacy trailing reserved word, creation/open/replacement actions
279 DosQueryFileInfo: FIL_STANDARD (1), also useful for the file regression
281 DosRead: regular files and synchronous standard input
Existing 224/256/282 (type/seek/write) now use the guest file-handle table.
All 15 ordinal imports in the supplied Infocom image now have dispatcher cases.
This means import coverage, not verified full interpreter compatibility.

Guest handles are small process-local integers, never truncated Win64 HANDLEs.
0/1/2 are borrowed host standard handles; closing them closes the guest view.
3..127 are file slots, reused after close. Runtime cleanup closes owned files.
FIL_STANDARD is written as 24 explicit bytes, independent of host packing.
Open action reflects CreateFile's result, avoiding a separate existence race.
Non-null EAs, unsupported open modes and information levels
are rejected. File sizes above the 32-bit status representation are rejected.
Allocation size in metadata is a 4 KiB-rounded approximation, not disk extents.
Common Win32 filesystem errors are returned directly; broad OS/2 error mapping
is not complete. ANSI path APIs are used, matching this historical runtime.

Deliberate remaining scope
--------------------------
No directory enumeration (DosFindFirst/Next/Close) yet. Infocom does not import
it; Sarien does. This is the first phase-2 file slice, not all directory APIs.
DosRead is synchronous: waiting for console input blocks the host WHP loop.
This is suitable for the single-threaded interpreter target; scheduler-aware
asynchronous input is still needed before claiming general threaded I/O.
No PM, queues, dynamic DLL lifecycle or new scheduling behaviour in this build.

Validation
----------
* Supplied Infocom passes the existing loader check: 368 internal records /
  753 internal sites, 28 external sites, 15 ordinal imports.
* Initial guest regression passed strict C89 syntax checking, but used incorrect
  handwritten API declarations. The corrected SDK-based fixture awaits a build
  with the historical OS/2 headers (see header/ABI correction below).
* Exact v2_fileio.h compiled with GCC -Wall -Wextra -Werror plus address and
  undefined-behaviour sanitizers against a POSIX Win32-API shim. Its file tests
  cover create/open/replace actions, reads/EOF, resize and position preservation,
  metadata, close/reuse, missing files, existing-file refusal, deletion and
  invalid guest buffers. Date values in this shim are synthetic.
* The shim is in tests/fileio-posix-check.c. From this package directory:
    gcc -std=c11 -Wall -Wextra -Werror tests/fileio-posix-check.c -o /tmp/fileio-check
    /tmp/fileio-check
  It uses /tmp/v2-fileio-unit.dat as its disposable test file.
* Full MSVC build, actual Win32 API behaviour, guest fileio1 execution and a real
  Infocom story run are NOT tested here and require the Windows machine.

R4 regression reminders
-----------------------
    whp_os2_v2_hi.exe hi.exe
    whp_os2_v2_hi.exe hi-lx.exe
    whp_os2_v2_hi.exe threadtort.exe 30
    whp_os2_v2_hi.exe sem1.exe
    whp_os2_v2_hi.exe semtort.exe 24
    whp_os2_v2_hi.exe dlltest.exe
    whp_os2_v2_hi.exe dllthread.exe 30
Both hi specimens intentionally exit 4; other tests should print PASS / exit 0.

R5 header/ABI correction
------------------------
fileio1.c now uses os2.h and its API naming/calling-convention mappings instead
of handwritten __cdecl declarations. Uses HFILE/ULONG/APIRET/DATETIME, writable
path buffers and eight-argument DosOpen calls as used in the V1 CMD source.
The host reads only those eight arguments, rejecting non-null EAs. It no longer
reads/rejects a ninth stack word: for an eight-argument caller that word is not
an argument and may contain arbitrary caller data. Older callers supplying an
extra reserved word remain accepted. This changes no stack cleanup convention.
Exact historical SDK compilation still requires the user's headers/toolchain.

R5b ABI8 diagnostic refresh
--------------------------
The supplied SDK os2.h selects INCL_32 under M_I386. os2def.h defines
APIENTRY as _syscall. bsedos.h declares an eight-argument DosOpen.
The supplied fileio1.exe agrees: at guest 00010076..00010095 it pushes
EA=0, mode=42, flags=10, attr=0, size=0, action pointer, handle pointer,
and path pointer; CALL at 00010095, ADD ESP,20h at 0001009A.
The guest ABI is therefore correct. The reported failure before the path
trace is consistent with an older host reading a ninth argument, but the
existing trace cannot establish which validation failed or which header
revision was compiled. Do not claim the filesystem create path is fixed
until the Windows regression passes.

Rebuild the host with v2_fileio.h beside the source:
    build-v2.cmd
    whp_os2_v2_hi.exe fileio1.exe 2>fileio-trace.txt
Look for the banner 'WHP OS/2 V2 R5b ABI8'. No guest rebuild is necessary.
DosOpen logs all eight argument words before validating them, names early
rejection reasons, and file/time dispatch logs return codes.
Host diagnostics now use stderr. Guest stdout continues through DosWrite:
    whp_os2_v2_hi.exe C:\OS2\demos\infocom\infocom.exe C:\os2\demos\infocom\planetfall 2>infocom-trace.txt
This keeps per-character tracing out of the game display. Guest stderr, if
used, shares that redirected stream. --check output also now uses stderr.

Offline file helper tests still pass with a nonzero ninth stack word.
The supplied guest binary has been disassembled; no WHP live test was possible
here. The prior Infocom log confirms Planetfall startup, command input and
quit confirmation, but no save operation. Its nonzero exit status remains
unexplained; no exit-code remapping is made in this update.
