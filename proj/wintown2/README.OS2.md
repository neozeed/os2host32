# WinTown/2: first Presentation Manager port

This is a new port of current WinTown, based on the commits recorded in
SOURCE_VERSION.txt. It uses the current WinTown simulation and Micropolis
submodule, with a separate C89 PM front end. The old working Micropolis port
is preserved under attic/hello-simcity and is not part of the active build.

## Target

32-bit Microsoft C386 (1991), OS/2 2.0 beta 6.78 headers, LINK386, and the
matching beta OS/2 resource compiler. The supplied headers are in include/os2h.
Use your existing libc.lib and os2386.lib. The original Makefile still builds
the upstream Windows front end; explicitly select Makefile.OS2 for this port.

From the project root, with compiler and libraries on PATH/LIB:

    nmake /f Makefile.OS2

Override tool names if necessary:

    nmake /f Makefile.OS2 CC=cl LINK=link386 RC=rc

For the split Windows-host / OS/2-VM workflow:

    nmake /f Makefile.OS2 code

Then in the OS/2 VM, from the same source directory:

    rc -r -i include\os2h wintownpm.rc
    rc wintownpm.res wintown2.exe

The code target deliberately produces an executable WITHOUT bound resources.
Bind resources before running it. A .res file is not an object module for
LINK386. The response file is wintown2.lnk; the module definition is
wintown2.def. Do not substitute the old Micropolis s.lnk/2.def files.

Run from the directory containing assets/, tilesets/, and cities/:

    wintown2.exe
    wintown2.exe cities\dullsville.scn

With the existing host shim, for example:

    \os2\os2host32.exe --run wintown2.exe

No executable or newly built PMWIN/PMGPI DLLs are included. Keep your last
working combined PM host overlay; this archive changes the game source.

## Implemented in this PM front end

- Current WinTown core, terrain generation, simulation, tools, disasters,
  scenarios, notifications, animation, and sprite movement.
- Map storage and access use WinTown's Map[x][y] convention.
- Default textured tileset, with classic fallback; external toolbar and sprite
  BMPs from current assets/. Manual 1/4/8-bit and RLE8 loading plus 24-bit RGB.
- Sprite transparency via magenta-key AND/OR masks, independent of palette
  indices. First eight sprite types use the same 61 frames as the NT assets.
- Double-buffered city view starting at a 640x300 frame; three independent
  city/log/info windows. R/C/I history is an additional initially hidden window.
- All 17 toolbar choices in three columns, signed demand bars, and white cursor
  footprints matching 1x1, 3x3, 4x4, or 6x6 tools.
- City Open and Save As path dialogs; all eight scenarios; speed and tool menus,
  keyboard accelerators; tax and funding controls with values in Info/Summary.
- Classic 27120-byte city loading/saving with explicit big-endian conversion,
  signed funds, histories, funding percentages, and complete tile flags.
  A malformed/truncated load does not replace the current city.

Left click builds or queries; right drag pans. Ctrl+N creates a city;
Ctrl+O opens; Ctrl+S saves as; Ctrl+Q exits. Ctrl+P/L/M/F select
pause/slow/normal/fast. Keys 1-8 select bulldozer, road, rail, wire,
residential, commercial, industrial, park. Budget funding menu items increase
by 25 percentage points, wrapping from 100% to 0%. Info shows current values.

The new-city action uses the generator defaults. Scenario maps are external
cities/*.scn files, rather than Windows RCDATA resources. Saving a scenario
creates a city snapshot; the classic city format does not retain scenario
objectives or active sprites.

## Remaining UI and target verification

This is a playable-core port candidate, not full NT UI parity. The native
new-city wizard, map overlays, zoom, tileset chooser, detailed evaluation UI,
full chart controls, rich notification dialogs, and sound are not ported.
Notifications appear in Log; the chart window currently shows R/C/I only.
Tools are click-to-place in this version; continuous left-drag construction
is not implemented. The path dialog accepts a filename rather than browsing
directories. New city/scenario commands replace the current city; save first.

The initial PM presentation, dialog resource grammar, LINK386 output, and
32-bit calling convention still need verification with your actual SDK and
OS/2/host runtime. There is no target C386 compiler or OS/2 runtime in this
build environment, so this package is source, not a claimed target-tested
binary. Host-side tests use native Linux scalar sizes, not a 32-bit ABI.

The display and classic city file use the same calendar counter: four ticks
per month, 48 per year. Old saves retain their stored counter; the old erroneous
displayed date cannot be recovered. See STAGE1-R2.txt and PORTING-PLAN.md for
this correctness update and the remaining stages. Scroll bars and keyboard
navigation from scroll-bar R1 are included.

## Diagnostics

Startup writes tile/icon/sprite load results to stderr. Missing or wrong-size
tilesets cause an explicit error instead of an empty brown/black window.
Routine game debug messages and periodic frame traces are disabled by default.
To enable them:

    set WINTOWN_TRACE=1

Capture stderr with your host's usual mechanism. To disable, remove the
variable (set WINTOWN_TRACE=). Host PM tracing has its own controls and can
still generate large output independently of this game's logging.

## Validation and source changes

Run the reproducible hosted C89 checks from a Unix-like development machine:

    sh tests/host-check.sh
    ASAN_OPTIONS=detect_leaks=0 CHECK_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' sh tests/host-check.sh

Verified: strict C89 syntax/build/link for the active core; syntax of PM code
against the actual beta headers; deterministic terrain with water and trees;
residential/road tools and funds; byte-level city format and state round trips;
truncated-file preservation; current bitmap decoding; all eight scenarios
with 512 simulation frames each; retired sprite cleanup. AddressSanitizer
and UndefinedBehaviorSanitizer completed these checks without reports after
the fixes below. Leak checking was disabled because this runner prevents
LeakSanitizer from reading its process threads. These checks do not render PM.

Two inherited memory errors found during that run were fixed:

- micropolis/src/sim/s_eval.c: wrap VoteProblems at x >= PROBNUM, avoiding
  an access one element beyond ProblemTable/ProblemVotes.
- sprite.c: retire frame-zero sprites before/after updates. Otherwise a dead
  monster could continue animating and index a movement table negatively.

Most root core changes are OS2_PM conditional includes and exclusions of
Windows-only presentation code. C89 fixes include explicit int declarations
and resource-header comments. The PM build avoids MS pack(push/pop) pragmas.
The s_sim_pm.c wrapper applies the same symbol renaming as upstream w_sim.h.
The original upstream NT main.c and Windows build remain present for reference.

The PM seam is os2pm/mp_port.h and mp_ui.h; the front end is os2pm_main.c;
city serialization is cityio.c; callback adapters are pm_bridge.c; bitmap
loading and mask creation are os2bmp.c. Do not edit attic to change this port.

Original upstream and attic licenses remain with their respective source trees.
