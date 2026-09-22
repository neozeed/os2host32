# Milestone 31B R1 — generic LE PM resources: WMCHAR menu + icon

M31A FINAL remains the frozen execution/PM baseline.  M31B R1 adds the first
reusable OS/2 Presentation Manager resource path, proven with the untouched
Microsoft OS/2 2.0 Beta-2 `WMCHAR.EXE`.

## What R1 adds

### LE/LX resource-table parsing in `os2host32`

The loader now reads the native `rsrc32` table from the executable header:

- resource type
- resource numeric id/name
- byte size
- owning object
- offset within that object

`--scan` reports the resources.  For the historical WMCHAR executable this is:

    Resources (2):
        type=1 id=1 size=1010 object=3 +00000000
        type=3 id=1 size=213  object=3 +000003F4

Type 1 is the OS/2 pointer/icon resource and type 3 is the PM menu template.

After the executable is mapped and fixed up, the loader publishes the main
module's resource descriptors to PMWIN through a private named host bridge.
This is executable-format plumbing only; PM semantics and translation stay in
PMWIN.

### Recursive OS/2 menu-template translation

PMWIN decodes the Beta-2 menu template recursively rather than hard-coding
WMCHAR IDs.  Supported in R1:

- `MIS_TEXT`
- `MIS_SEPARATOR`
- `MIS_SUBMENU`
- `MIS_SYSCOMMAND`
- `MIA_CHECKED`
- `MIA_DISABLED`
- OS/2 `~` mnemonic -> Win32 `&` mnemonic

WMCHAR's resource decodes to the original two top-level menus:

    Actions
        Clear List
        Log KeyUps
        --------
        Exit (SC_CLOSE / MIS_SYSCOMMAND)

    Display
        Number
        Virtual Key
        Character
        Scancode
        Repeat Count
        Flags

`WinWindowFromID(frame, FID_MENU)` now returns the native translated menu.
`WinSendMsg(MM_SETITEMATTR)` recursively locates menu items, so WMCHAR's
existing `CheckMenus()` logic can set/check the original menu state.

Native menu commands are translated back to guest `WM_COMMAND`.  Resource
items marked `MIS_SYSCOMMAND` are handled as frame/system actions; WMCHAR's
`SC_CLOSE` therefore closes the frame as it did under PM.

### OS/2 color-icon translation

R1 also translates the Beta-2 type-1 bitmap-array/color-icon resource used by
WMCHAR into a native Win32 `HICON` and assigns it to the frame.  WMCHAR's icon
resource in the LE image is byte-identical to the supplied historical
`WMCHAR.ICO`.

The first icon translator supports the OS/2 2.x `BITMAPINFOHEADER2` color-icon
shape used by this SDK sample (1-bpp AND/XOR mask plus 1/4/8-bpp color plane).
It is intentionally format-driven, not WMCHAR-specific.

## Static regression

Run:

    make m31b-wmchar-resource-check

or the full gate:

    make m31b-wmchar-check

The resource regression requires:

- exactly the historical `(RT_POINTER,1)` and `(RT_MENU,1)` resources,
- icon resource byte-identical to `WMCHAR.ICO`,
- the 213-byte CP850 menu template,
- all 12 expected decoded menu records including recursion and `SC_CLOSE`.

## Windows visual/functional test

Build as usual:

    make clean
    make tools compat
    make m31b-wmchar-check

Then:

    set OS2_PM_TRACE=1
    os2host32.exe --run examples\m31a-wmchar\WMCHAR.EXE

or run `m31b-wmchar-test.cmd`.

Expected new M31B behavior on top of the already-proven M31A behavior:

1. The **Actions** and **Display** menus appear in the native frame.
2. Number, Virtual Key, Character, and Flags are initially checked by the
   original WMCHAR `CheckMenus()` code.
3. Display items can be toggled and the corresponding columns repaint.
4. Log KeyUps toggles its check mark and enables/disables key-up logging.
5. Clear List clears the captured rows.
6. Exit closes the application.
7. The original WMCHAR icon is used for the frame where the Windows shell
   chooses to display it.
8. Typing/scrolling/resizing from M31A continues to work.

Useful trace lines are:

    PM resources     : published 2 main-module resources
    PMWIN: resource menu OK ...
    PMWIN: resource icon OK ...
    PMWIN: WinWindowFromID menu ...
    PMWIN: WinSendMsg ... 00000192 ...

## R1 limits

- Main-executable resources are published; guest-DLL resource publication is
  future work when a sample requires it.
- Menu text is byte-oriented.  The template code page is decoded/validated,
  but R1 does not yet perform general CP850 -> Unicode conversion.  WMCHAR's
  resource text is ASCII, so this is lossless here.
- Owner-draw/bitmap/help/multiple-choice menu styles are not implemented yet.
- The icon translator intentionally implements the OS/2 2.x color-icon shape
  actually emitted by the Beta-2 SDK sample first.

These are format/feature limits, not WMCHAR special cases.
