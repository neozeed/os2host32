# PMWIN mouse R3

The supplied PM mouse probe exposed two missing pieces in PMWIN.dll:

1. The native client and dialog window procedures translated WM_MOUSEMOVE and
   left-button messages only. Right and middle button down/up/double-click
   messages fell through to the native window procedure instead of the guest.
2. WinGetKeyState inverted the WM_CHAR keyboard map, which deliberately omitted
   mouse virtual keys, so VK_BUTTON1/2/3 polling returned zero.

Both callback paths now use the same three-button map:

| Windows messages | OS/2 PM messages |
| --- | --- |
| WM_LBUTTONDOWN / UP / DBLCLK | WM_BUTTON1DOWN / UP / DBLCLK, 0x71..0x73 |
| WM_RBUTTONDOWN / UP / DBLCLK | WM_BUTTON2DOWN / UP / DBLCLK, 0x74..0x76 |
| WM_MBUTTONDOWN / UP / DBLCLK | WM_BUTTON3DOWN / UP / DBLCLK, 0x77..0x79 |

WinGetKeyState now maps OS/2 VK_BUTTON1/2/3 explicitly to Windows VK_LBUTTON,
VK_RBUTTON and VK_MBUTTON. OS/2 VK_BUTTON3 is 0x03 whereas Windows VK_MBUTTON
is 0x04; the raw numeric key must not be passed through as VK_CANCEL.
Keyboard key-state queries retain their existing mapping.

## Install and test

Extract PMWIN.dll from PMWIN-MOUSE-R3-win32.zip and replace the PMWIN.dll selected
by your loader/Soft386 (normally alongside os2host32.exe in C:\os2). Close the
existing probe and other running compatibility PM processes before replacing it,
then start the same PMMOUSE executable again. It needs no recompilation.

Right-click inside the client: expect WM_BUTTON2DOWN with state R1 and VK2=1,
followed by WM_BUTTON2UP with state R0 and VK2=0. Right double-click should
produce WM_BUTTON2DBLCLK. Middle clicks should show WM_BUTTON3 messages and VK3.
The probe captures on button down, so release outside the client should still
reach it. This change enables those events; it does not add a capture policy.

Captain Blood can then be retested. This diagnoses and fixes the missing host
mouse events, but its game-specific behaviour is not proven here.

Existing mouse-coordinate conversion and mp2=0 behaviour are preserved. Mouse
modifier/hit-test packing, wheel/X buttons and other input extensions are not
part of this patch. WinSetPointerPos ordinal 867 remains in the rebuilt DLL.

## Source and build

PMWIN-MOUSE-R3.patch applies at the repository root after PMWIN867-VIOFETCH R1,
with or without the subsequent Soft386 jar-memory R2 patch:

    git apply PMWIN-MOUSE-R3.patch
    make PMWIN.dll MINGW=i686-w64-mingw32-gcc
    make pm-mouse-check pm-pointer-check

The full R3 source archive already includes PMWIN867/VIOFETCH R1 and jar-memory
R2. Do not apply the patch to that archive again. This follow-up modifies only
PMWIN, its Makefile test target, and tests/documentation. DOSCALLS, VIO, NLS, MSG,
Soft386's execution machinery, and WHP are unchanged from the R2 source package.

## Validation

- Actual PMWIN client/dialog mouse switch branches, message-map helper,
  coordinate helper, keyboard map and WinGetKeyState export compiled against
  deterministic Win32 mocks: PASS. All ten move/button message mappings, signed
  captured coordinates, button up/down state, middle-button numeric mismatch,
  missing-dialog guard and keyboard polling regression are covered.
- Existing pointer-position host checks: PASS.
- 32-bit MinGW PMWIN.dll build: PASS; libgcc linked statically. Imports only
  Windows system DLLs. Export ordinal 867 remains nonzero.
- Patch applies cleanly to both R1 and R2 source baselines and reproduces changed
  files byte for byte. ZIP integrity verified.

Live Windows probe execution with this DLL remains to be checked on your machine.
