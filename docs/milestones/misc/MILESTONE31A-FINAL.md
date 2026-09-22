# Milestone 31A FINAL — WMCHAR

Frozen: 2026-09-18

Milestone 31A is the first SDK-example expansion of the native Win32
Presentation Manager compatibility path after Milestone 30.

## What is proven

The untouched Microsoft OS/2 2.0 Beta 2 / 6.78 SDK `WMCHAR.EXE` now runs as a
32-bit LE guest through `os2host32` on Win32/Win64 hosts.  It:

- initializes PM and a message queue;
- registers its original guest window procedure;
- creates and shows a native peer window;
- paints its original column headings;
- repaints correctly after resize/expose;
- receives translated keyboard input;
- records and draws successive WM_CHAR rows;
- scrolls the client area as new rows arrive;
- continues to coexist with the earlier Sarien PM regression.

The historical executable in `examples/m31a-wmchar/WMCHAR.EXE` is preserved
unchanged.  SHA-256:

    59965118f37df6f1058f934e38777a4f36627cf22da24c6f708fe0470c9d9d36

## Important architecture discoveries

### 1. LINK386 internal OFF32 targets are address arithmetic

M31A found a valid LINK386 internal OFF32 source-list relocation to object 2 +
`FFFFF9F0`.  The target offset is a signed/biased address constant used by
indexed addressing; it is not required to lie inside the declared target
object.  Internal flat OFF32 relocation therefore uses 32-bit wraparound
arithmetic instead of rejecting target offsets outside the object extent.

The historical WMCHAR executable permanently guards this in
`tools/check_m31a_wmchar_fixups.py`.

### 2. Native PM calls need more stack than a 1989/1990 guest expected

WMCHAR's combined data/stack object is only `0x2C80` bytes (~11 KB).  Running
modern USER32/GDI synchronously on that same x86 stack exhausted the historical
stack headroom and overwrote live guest globals before the message loop began.
The apparent keyboard, FONTMETRICS, sprintf, and relocation failures seen in
R7-R12 were downstream symptoms of that overwrite.

For eligible non-DLL PM programs whose initial ESP is exactly the end of a
small stack object, `os2host32` now extends the runtime stack mapping to
`0x40000` bytes (256 KB) and starts ESP at the promoted top.  The guest's
original LE object size, file-backed pages, preferred addresses, globals, and
fixups remain unchanged.  Promotion is refused if it would overlap another
object.

WMCHAR therefore changes at runtime from:

    object 2 + 00002C80   (~11 KB)

to:

    object 2 + 00040000   (256 KB runtime stack span)

Sarien is intentionally unaffected because its historical stack is already
larger than the promotion threshold.

### 3. PM host boundaries should not pass guest text directly to GDI

`WinDrawText` copies the explicit guest byte sequence to host-owned memory
before calling GDI.  This makes the guest/host boundary clearer and avoids
letting native GDI consume pointers into guest stack/data directly.

### 4. Win32 activation/IME traffic is not OS/2 PM traffic

Visible window creation is deferred until the first `WinGetMsg`, after the
guest has completed post-create PM setup.  Windows IME messages are suppressed
for PM guest windows; OS/2 keyboard input continues through the explicit
Win32-keyboard-to-OS/2-WM_CHAR bridge.

## PM surface added/expanded in M31A

M31A extends PMWIN with the WMCHAR-required APIs including window/query,
painting, scrolling, text drawing, PS acquisition/release, invalidation, and
menu-message scaffolding.  PMGPI adds `GpiQueryFontMetrics`.  PMSHAPI is now a
host personality with `WinAddSwitchEntry @120`.

The resource menu/icon template itself is not yet decoded.  WMCHAR therefore
runs without its original OS/2 menu.  That is the intended next fidelity area,
not a blocker for the M31A execution milestone.

## Regression commands

Build:

    make clean
    make tools compat

Static/import regression:

    make m31a-final-check

Interactive regression:

    m31a-final-test.cmd

Expected interactive result: a `Char Messages` window with the original column
headings.  Typing printable characters adds numbered rows and scrolls them.
Arrows/function keys exercise the virtual-key path.  Resizing must repaint
without corrupting guest state.

For verbose PM diagnostics only:

    set OS2_PM_TRACE=1
    os2host32 --run examples\m31a-wmchar\WMCHAR.EXE

## Freeze rule

Treat this tree as the frozen M31A baseline.  Do not remove the stack-promotion
rule, LINK386 OFF32 behavior, guest-text host copy, or WMCHAR regression while
starting M31B.  New PM work should preserve both WMCHAR and Sarien behavior.
