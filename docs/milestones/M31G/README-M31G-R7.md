# M31G R7 - PMWIN.909 WinCreateWindow for NEKO

R6 proved `WinLoadPointer` in the real Win32 loader path: all 26 PMWIN.780
fixups resolve, followed by existing PMWIN.716 and PMWIN.781.  The untouched
OS/2 2.0 GA NEKO.EXE then stops at its next unresolved import:

    PMWIN.909

Historical PM ordinal material identifies 909 as the ordinary 13-argument
`WinCreateWindow` API.

## What the untouched NEKO call actually does

The single PMWIN.909 call site in the frozen NEKO binary creates a desktop
`WC_STATIC` control with `SS_ICON`, id 1 and text `"#1"`, at OS/2 position
(20,20).  Its width/height come from application data.  The caller supplies
`HWND_TOP`, then immediately calls the already-imported `WinSubclassWindow`
on the returned handle.

This is useful evidence, but R7 does not special-case NEKO.  It implements the
normal PM public-control path.

## Generic WinCreateWindow mapping

R7:

- exports `WinCreateWindow @909 NONAME`;
- recognizes the OS/2 predefined `0xffff000N` WC_* public class atoms;
- maps the common button/static/entry/listbox/scrollbar/combobox classes to
  their native Win32 peers;
- keeps registered private classes on the existing `pm_wndproc` path;
- translates the already-used OS/2 `WS_VISIBLE`, tab/group and control-style
  bits;
- converts PM parent-relative lower-left coordinates to Win32 upper-left
  coordinates using the parent client height, or screen height for
  `HWND_DESKTOP`;
- maps OS/2 `HWND_TOP`/`HWND_BOTTOM` placement to native z-order operations;
- treats `WC_STATIC | SS_ICON` text of the form `"#N"` as an application
  `RT_POINTER` resource reference and installs the existing OS/2->Win32 icon
  conversion into the native STATIC peer.

The last point is important architecturally: passing `"#1"` straight to a
Win32 `STATIC/SS_ICON` window would make USER32 look for resource 1 inside the
host PMWIN.dll.  OS/2 means the guest executable's resource instead.

Unsupported public frame/menu/titlebar classes currently return NULLHANDLE
rather than fabricating behavior.

## Build / test

    make PMWIN.dll os2host32.exe
    make m31g-neko-r7-check
    os2host32.exe --run examples\m31g-neko\NEKO.EXE

or:

    examples\m31g-neko\m31g-neko-r7-test.cmd

R7 success is that PMWIN.909 resolves and execution reaches the next genuine
boundary.  Do not pre-implement the next missing import from static ordering.
