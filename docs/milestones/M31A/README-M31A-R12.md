# M31A R12 - native PM stack headroom

R11 proved that WMCHAR's Beta-2 FONTMETRICS layout and the loader relocations
for its metrics globals, `%u.` format string, `gacm[]`, and virtual-key table
are correct.  It also exposed a more fundamental native-hosting constraint:
WMCHAR's data/BSS and stack share object 2, whose historical initial ESP is
only `0x2C80` bytes from the object base.  The active `gcxMaxChar/gacm[]`
globals begin around object 2 + `0x0BD4`, leaving only `0x20AC` (8364) bytes
between live global state and the top of the original stack.

That was sufficient for OS/2 2.0-era Presentation Manager, but os2host32 V1
executes modern Win32 USER32/GDI calls synchronously on the guest x86 stack.
Those host call chains can consume substantially more than eight kilobytes and
silently overwrite the guest data/BSS below the stack.  This matches the R11
symptoms: metrics are initially correct, then WMCHAR later observes corrupted
character geometry and formatted-message state after native window/GDI work.

R12 adds a general native-PM stack policy:

* only non-DLL executables importing PMWIN/PMGPI are considered;
* only a small stack whose initial ESP is exactly the end of its stack object
  is eligible (we never move ESP through declared object data);
* the extension is rejected if it would overlap any other LE/LX object;
* eligible stacks smaller than 256 KB are mapped/committed out to object
  offset `0x40000`, and initial ESP is raised to that offset;
* original object size, file pages, fixups, and preferred data/BSS addresses
  are unchanged;
* large PM programs are untouched.  In particular, Sarien's ~383 KB stack is
  already above the threshold and is not promoted.

R12 also changes WinDrawText's guest-text isolation buffer from a 512-byte C
local (which still lived on the guest ESP) to a true Windows process-heap
allocation.

Host-independent regression:

    python tools\check_m31a_wmchar_stack.py examples\m31a-wmchar\WMCHAR.EXE

Expected WMCHAR runtime diagnostic:

    M31A PM stack   : promoted object 2 ESP 00002C80 -> 00040000 (256 KB runtime stack span)
    M30D TIB stack   : 01020000..01060000 (256 KB)

Run:

    make clean
    make tools compat
    make m31a-wmchar-check
    set OS2_PM_TRACE=1
    os2host32.exe --run WMCHAR.EXE

A useful behavioral check is to resize the window and type several printable
keys.  With the corruption removed, WinScrollWindow should use a vertical delta
near `-16` rather than the corrupted values seen in R11, and the dynamic text
buffer should contain printable message-number text rather than a control byte.
