# M31C R1 - HANOI first runnable PM pass

Target: the untouched Microsoft/IBM OS/2 2.0 Beta 2 SDK `HANOI.EXE`.

This pass extends frozen M31B without changing the proven WMCHAR resource path.

## New HANOI API surface

PMGPI:
- 356 `GpiBox`
- 519 `GpiSetCurrentPosition`
- 517 `GpiSetColor` was already present and is used by HANOI.

PMWIN:
- 701 `WinAlarm`
- 729 `WinDismissDlg`
- 789 `WinMessageBox`
- 814 `WinQueryDlgItemShort`
- 858 `WinSetDlgItemShort`
- 903 `WinSendDlgItemMsg`
- 910 `WinDefDlgProc`
- 919 `WinPostMsg`
- 923 `WinDlgBox`

DOSCALLS:
- 232 `DosEnterCritSec`
- Corrected 234 `DosExit`: `EXIT_THREAD` now terminates only the caller thread;
  `EXIT_PROCESS` retains process-wide cleanup/termination.

## Resources

M31B already publishes generic LE resources. R1 consumes HANOI's additional
resource types:

- type 1 / id 1: application/dialog icon
- type 3 / id 1: Action / Options menu
- type 4 / id 6: Set disk count dialog
- type 4 / id 9: About dialog
- type 8 / id 1: accelerator table (`Alt+S -> IDM_SET`)

The type-4 parser consumes the documented packed OS/2 `DLGTEMPLATE` / `DLGTITEM`
format and maps the control subset HANOI uses: static text/icon, entry fields,
push buttons and default push buttons.

## Expected test

Run `m31c-hanoi-r1-test.cmd`.

Expected first milestones:
1. HANOI window opens with its resource icon and Action/Options menu.
2. Three posts/base and five disks paint in OS/2 bottom-left coordinates.
3. Action -> Start runs the recursive worker thread and moves disks.
4. Completion posts `UM_CALC_DONE` back to the main PM thread and shows Done.
5. Options -> Set and Alt+S open the disk-count dialog; OK updates the tower.
6. Invalid values (for example 17) show the original warning message box.
7. Action -> About shows the resource-backed About dialog and icon.
8. Stop disables the recursive calculation through the original shared flag.

M31B WMCHAR remains the regression gate and must stay green.
