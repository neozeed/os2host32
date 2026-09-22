# Milestone 31B FINAL — generic OS/2 PM resources on Win32

Milestone 31B is frozen from the exact M31B R1 implementation that was
manually proven on Windows with the untouched Microsoft OS/2 2.0 Beta-2
`WMCHAR.EXE`.

M31A FINAL remains the execution/PM baseline.  M31B adds a reusable resource
path for main-module LE Presentation Manager resources.

## Proven result

The untouched historical WMCHAR executable now runs through os2host32 and uses
its original embedded OS/2 resources on a native Win32 frame:

- original application icon appears in the frame and Windows taskbar,
- original `Actions` and `Display` menus are reconstructed from the LE menu
  template,
- original WMCHAR code sets and updates menu check marks,
- Display toggles add/remove Number, Virtual Key, Character, Scancode, Repeat
  Count, and Flags columns,
- Log KeyUps toggles key-up capture,
- Clear List works,
- Exit / `MIS_SYSCOMMAND` / `SC_CLOSE` works,
- keyboard, painting, scrolling, resizing, CRT formatting and M31A stack
  promotion continue to work.

This proves both directions of the resource/menu bridge:

    OS/2 LE resource -> native Win32 HMENU/HICON
    native menu command -> original guest WM_COMMAND / system action

## Generic implementation added in M31B

### Loader

`os2host32` parses LE `rsrc32` entries and publishes mapped main-module
resource descriptors to PMWIN.  The parser is driven by resource type/id,
object and offset; no WMCHAR menu IDs are hard-coded in the host.

For the frozen sample:

    type 1 / id 1 : 1010-byte OS/2 pointer/icon resource
    type 3 / id 1 :  213-byte PM menu template, codepage 850

### PMWIN menu translator

The recursive Beta-2 menu-template translator supports the forms needed by
this sample:

- `MIS_TEXT`
- `MIS_SEPARATOR`
- `MIS_SUBMENU`
- `MIS_SYSCOMMAND`
- `MIA_CHECKED`
- `MIA_DISABLED`
- OS/2 `~` mnemonic conversion to Win32 `&`

`WinWindowFromID(frame, FID_MENU)` exposes the translated native menu to the
guest, while `WinSendMsg(MM_SETITEMATTR)` recursively locates and updates
items.  Native menu selections are translated back to guest PM commands.

### PMWIN icon translator

The type-1 translator handles the OS/2 2.x color-icon form emitted by this SDK
sample (`BITMAPINFOHEADER2`, AND/XOR mask plus color plane) and creates a
native Win32 `HICON`.

## M31A behavior retained

The M31A tiny-PM-stack promotion remains part of the baseline.  WMCHAR's
historical ~11 KB stack/data object is promoted to a 256 KB runtime stack span
without moving its initialized data or changing the executable on disk.  This
prevents modern USER32/GDI stack usage from overwriting guest globals.

The LINK386 signed/biased OFF32 compatibility and the WMCHAR relocation
regressions remain required.

## Final regression gate

On the Windows build host:

    make clean
    make tools compat
    make m31b-final-check

Then run:

    m31b-final-test.cmd

The automated gate includes the frozen M31A checks plus the M31B resource
parser/menu regression.  The GUI portion remains a manual Win32 integration
check.

## Known limits carried forward

- Only main-executable resources are published today; DLL-resource publication
  is deferred until a sample requires it.
- General CP850-to-Unicode menu conversion is not implemented; WMCHAR menu
  text is ASCII and therefore lossless.
- Owner-draw, bitmap, help and uncommon menu styles are not implemented yet.
- The icon translator intentionally implements the OS/2 2.x color-icon shape
  proven by this SDK sample first.

These are generic feature limits, not WMCHAR special cases.

## Freeze rule

Do not modify M31B FINAL to pursue the next sample.  Future PM work starts from
this tree as Milestone 31C, with HANOI as the next intended SDK target.
