WHP OS/2 V2 R12 - first Sarien integration attempt
================================================
R12 FIX 2 - preserve Sarien's 320x200 client area
User confirmed SQ2 runs, beeps, and restores saved games after fix 1.
Side-by-side screenshots then exposed clipped top menus and the right edge.
Sarien requests width 320 and height 200+SV_CYTITLEBAR-2 for its frame.
Passing these directly as Windows outer dimensions subtracts native borders
and caption from the intended client image. Bottom-aligned GPI output then
clips at the top as well as the right.

run-sarien.cmd now also sets WHP_OS2_SARIEN_GEOMETRY=1 inside SETLOCAL.
For frame sizing with that setting, the host derives the intended client
height and uses AdjustWindowRectEx with the window's actual styles to obtain
native outer dimensions. This follows the earlier runner's Sarien sizing
convention, scoped explicitly to the launcher. Default frame sizing and
client-handle requests retain their existing behavior. Move-only calls do
not apply this conversion. No bitmap rescaling or guest binary edit is used.

Rebuild with build-v2.cmd, then use the updated run-sarien.cmd with the same
arguments as before. Look for "Sarien geometry: client=320x200" in the trace.
Confirm the entire top menu and right edge are visible during play/restore.
For manual launches, also set WHP_OS2_SARIEN_GEOMETRY=1; clear it afterward.

Native ASan/UBSan checks pass for the 320x200 client with nonzero simulated
borders/caption, bottom-origin placement, size-only/move-only calls, default
mode, client handles and adjustment failures. Existing scheduler/PM/GPI
checks pass in the same run; see validation/r12-geometry-fix.*.
CONFIRMED on Windows by the user, 2026-09-20: complete status bar and right
edge visible; trace reports client=320x200, native frame=336x239, exit rc=0.
See MILESTONE1.md for the final status; later prospective wording in this
historical release note describes the original validation stage.

R12 FIX 1 - Sarien's uninitialized find count
The supplied live trace showed the first DOSCALLS.264 returning 87, followed
by successful *.* and *vol.0 searches. Disassembly of sarienlx.exe at guest
0x16028 confirms the existence helper passes the uninitialized DWORD at
EBP-4 as its in/out count; it returns true only when the output is nonzero.
A zero first count therefore rejects v2, while a subsequent nonzero stack
value permits v3 detection. The resulting v3 CRC sums different files, and
its loader then requests the nonexistent dir. file.

run-sarien.cmd now sets WHP_OS2_FIND_ZERO_COUNT=1 within SETLOCAL. This explicit
compatibility setting lets DosFindFirst treat a zero input count as one.
Other arguments, buffer bounds, filters and failures are still validated;
missing matches still return no files. DosFindNext and default strict behavior
are unchanged. Each first search now logs path/count/buffer size/attributes/
level, plus any compatibility fallback. There is no fabricated game match.
Sarien's existing binary and game data are not modified.

Rebuild the host with build-v2.cmd and use the updated run-sarien.cmd.
For a manual launch, first run: set WHP_OS2_FIND_ZERO_COUNT=1
Clear it afterward with: set WHP_OS2_FIND_ZERO_COUNT=
Native directory/file dispatcher checks passed under ASan/UBSan for strict
mode, opt-in zero-count success, missing files, invalid sizes/levels and
unchanged FindNext behavior. See validation/r12-find-fix.*. The subsequent user run reached SQ2 gameplay with sound and working restore.

Cumulative source package based on user-verified R11, including its corrected
16-entry palette fixture. Live result supplied by the user:
  gpi1 PASS (3 paints; bitmap readback, copy and cleanup)
Screenshots confirmed orientation, palette colors, scaling and window title.

R12 fills the remaining imported calls and graphics behaviors identified
from the supplied sarienlx.exe import scan and the older sarienPM.zip source,
especially nullvid.c. That archive predates the executable's queue/audio
imports, so the binary remains the final integration test. The interpreter
executable is unchanged. No Windows host or new guest executable is prebuilt.

BUILD AND TRY
  build-v2.cmd                  (x64 MSVC developer prompt)

Optional short focused check, using your C/386 environment:
  build-c386-sarprep.cmd
  run-sarprep.cmd --auto
Expected:
  sarprep1 PASS (<n> worker frames, <n> ticks, <n> paints, <n> keys)
The automatic run checks exactly 40 worker frames; tick/paint counts vary.
A brief 880 Hz tone is requested while another guest thread keeps ticking.

Interactive variant:
  run-sarprep.cmd
Shows an animated black/cyan/magenta/white checkerboard for about a second,
then leaves the last frame visible. Press arrow keys: the console should
report virtual values 21/22/23/24 for left/up/right/down. Printable keys
report their character value. Escape or the close button ends the test.
Diagnostics: sarprep1-trace.txt, overwritten each run.

To try the actual Sarien binary, put sarienlx.exe beside the host, then:
  run-sarien.cmd C:\path\to\your\AGI-game
Or specify its existing location:
  run-sarien.cmd C:\path\to\your\AGI-game C:\sar\sarienlx.exe

The script changes to the game directory and launches the host/guest by
absolute path. This matters because the supplied source discovers the game
from its current directory. It does not assume the first guest argument is
a game path; the older PM wrapper uses that argument for frame skipping.
Host diagnostics go to sarien-trace.txt beside the script; guest console
output remains visible. No game data or interpreter binary is bundled.
The equivalent manual command from inside a game directory is:
  C:\proj\whp\whp_os2_v2_hi.exe C:\sar\sarienlx.exe 2>sarien-trace.txt

For the first real run, check the initial game screen, typing, arrow movement,
Escape/menu interaction and closing the window. If it stops, send the console
output and sarien-trace.txt. Trace records identify failed/unsupported calls.
There is no claim here that Sarien itself has run on WHP yet.

NEW CALLS
  PMWIN.746 WinFocusChange
  PMWIN.794 WinOpenWindowDC
  PMWIN.829 WinQuerySysValue
  PMWIN.875 WinSetWindowPos
  DOSCALLS.286 DosBeep
The existing directory, file, memory, mutex, thread and QUECALLS paths cover
the remaining imported base services in the supplied scan.

SARIEN-SPECIFIC BEHAVIOR NOW COVERED
- WinOpenWindowDC returns an opaque guest window-DC token. GpiCreatePS accepts
  it to make a persistent window PS, as well as the existing memory PS.
- GpiBitBlt to that PS works outside BeginPaint, including from the interpreter
  worker thread. A native DC is acquired/released for that individual call.
  During a matching paint operation, it reuses the current paint DC.
- WinBeginPaint accepts the caller's window PS; WinEndPaint accepts the same
  HPS. The R10/R11 zero-HPS path continues to work.
- GPI presentation spaces are shared within the guest process. One VP executes
  guest code at a time, and Sarien's guest mutex serializes bitmap updates
  against main-thread painting. GPI calls do not themselves yield or invoke
  guest code. This is cooperative scheduling, not native parallel GPI access.
- GpiCreateLogColorTable supports LCOLF_CONSECRGB with up to 256 supplied RGB
  entries and the PURECOLOR flag. Indexed GpiSetColor resolves through that
  logical table. Selected indexed bitmap palettes receive the table, allowing
  Sarien's SetPel -> QueryBitmapBits sequence to discover its color indices.
- GpiQueryBitmapInfoHeader supports the requested 16-byte prefix. It writes
  exactly 16 bytes and retains cbFix=16, rather than overrunning the caller
  with a full header. Query/SetBitmapBits support the corresponding RGB2 table
  immediately after that prefix. Existing 12/64-byte headers remain supported.
- Repeated WinInitialize on the PM owner is accepted. Secondary threads can
  initialize/terminate an anchor without destroying the owner's queue/window.
  Secondary anchors have distinct guest tokens. Shared GPI calls may use
  existing PSs even when the worker has not initialized its own anchor, as in
  the older Sarien source. Creating new PSs still uses the main process HAB.
- Worker PostMsg/PostQueueMsg can wake the owner's queue. Cross-thread
  synchronous SendMsg and multiple independent PM window trees remain deferred.
- Arrow, Home/End, Page Up/Down, Insert/Delete and F1-F12 events translate to
  OS/2 virtual keys, including key-up. Characters arrive through WM_CHAR;
  Escape, Enter and Backspace also carry their OS/2 virtual value. Shift/Ctrl/
  Alt flags are sampled. This is not full keyboard-layout/dead-key support.
- WinQuerySysValue covers screen dimensions, size borders, borders and title
  height. WinSetWindowPos implements the needed SHORT-coordinate move/size
  path and basic show/hide/redraw/activation flags; Y is converted from the
  OS/2 desktop's bottom-left convention. WinFocusChange targets the client.

TONE IMPLEMENTATION
DosBeep validates 37..32767 Hz and supports durations up to 60000 ms. A zero
duration succeeds immediately. A short-lived native worker calls Windows
Beep; the guest caller sleeps through the existing scheduler for the requested
duration, allowing other guests and the Windows message pump to progress.
Tone workers own no Runtime or guest-memory pointers. Their thread handles
are closed immediately; completed workers free their own job storage. Up to
32 can be active, with allocation/capacity failures returning an error.

Native audio is best effort: Windows Beep failures are logged, and guest
sleep timing remains intact. Guest completion follows the requested deadline,
not a confirmed hardware playback completion. This is not the persistent
WinMM PCM implementation used by V1, so audible quality/gaps may differ. Audio
workers end with the host process; this adds no new link-library requirement.

FOCUSED GUEST TEST
sarprep1.c follows Sarien's sequence: standard-window WM_CREATE creates the
window/memory PSs, selects an 8-bpp bitmap and installs 33 logical colors.
SetPel probes are read back through a 16-byte bitmap header and checked
against the returned palette. A painter thread repeatedly uploads and blits
under a guest mutex, outside WM_PAINT, while a ticker thread sleeps/runs.
The paint callback uses the supplied window PS and the same mutex. The tone
check verifies ticker progress while the painter is blocked. The automatic
close is posted from the worker. Main joins both threads before destroying
graphics resources, the mutex, window, queue and anchor.

VALIDATION PERFORMED HERE
- Production helpers pass ASan/UBSan execution with Windows/WHP doubles:
  logical colors, short-header bounds, palette roundtrip, shared window PS,
  supplied paint PS, worker posting, anchors, native coordinate conversion,
  arrow/key-up/Escape/character translation, and guest timing during Beep.
- Existing R7-R11 native scheduler/info/mutex/queue/callback/PM/GPI regression
  checks pass in the same suite. Logs: validation/r12-native-check.*
  Reproduce with GCC on Linux:
    ASAN_OPTIONS=detect_leaks=0 python3 tests/run-sync-check.py
  LeakSanitizer is disabled for the ptrace-based execution environment.
- sarprep1.c, gpi1.c and pmhello.c pass GCC -m32 C89 type/syntax checking against
  the supplied Beta2 SDK, with legacy keywords removed and existing SDK
  comment/pragma warnings suppressed. This does not claim a C/386 build/link.
- No actual MSVC build, Windows rendering/audio, or WHP execution was possible
  here. Source inspection/native doubles cannot establish that the compiled
  Sarien variant follows every assumption; its first live trace is needed.

BOUNDARIES RETAINED
One PM owner/window pair; no general controls/resources, mouse input, arbitrary
GPI primitives/fonts, other raster ops, FPU/SIMD thread-context switching or
preemption of compute-only guest loops. Sarien's inspected timer loop calls
DosSleep, so it cooperates with the scheduler. Unsupported calls/modes remain
visible in traces. Previous release notes are historical and some of their
listed limitations are superseded by the behavior above.
