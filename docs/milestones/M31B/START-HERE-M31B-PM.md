# Start here — Milestone 31B Presentation Manager

M31A is frozen and proven.  The untouched Microsoft Beta-2 `WMCHAR.EXE` paints,
resizes, accepts keyboard input, formats rows, and scrolls correctly through
the native Win32 PM compatibility path.

## Baseline that must remain green

Before and after M31B changes:

    make clean
    make tools compat
    make m31a-final-check
    m31a-final-test.cmd

Also keep the existing Sarien PM regression working.

## Recommended M31B target

Implement general OS/2 PM resource handling needed by WMCHAR's original
resource menu/icon rather than hard-coding this application.

WMCHAR currently asks for frame controls including menu/icon through
`WinCreateStdWindow`, and subsequently calls:

- `WinWindowFromID(frame, FID_MENU)`
- `WinSendMsg(menu, MM_SETITEMATTR, ...)`

The current compatibility layer intentionally has no OS/2 resource-template
translator, so `FID_MENU` returns no native menu.  M31B should decode enough
LE/PM resource metadata to create the menu generically and map menu commands
back to the guest window procedure as OS/2 `WM_COMMAND`.

The original WMCHAR resources are preserved in:

    examples/m31a-wmchar/WMCHAR.RC
    examples/m31a-wmchar/WMCHAR.ICO
    examples/m31a-wmchar/WMCHAR.EXE

Do not hard-code WMCHAR menu IDs in PMWIN.  Use them only as the first
regression for a reusable resource/menu implementation.

## Architectural rules carried forward

- V1 remains direct 32-bit LE/LX execution with Win32 personality DLLs.
- M30 EMX work remains frozen.
- The separate WHP V2 proof remains frozen for later 16-bit/transition work.
- PM semantics belong primarily in PMWIN/PMGPI/PMSHAPI; loader changes should
  be limited to executable-format/resource knowledge genuinely required by PM.
- Preserve the M31A tiny-PM-stack promotion and LINK386 OFF32 semantics.
- Add each SDK sample as a permanent regression instead of replacing WMCHAR.
