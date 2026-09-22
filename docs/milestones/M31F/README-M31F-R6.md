# M31F R6 - JIGSAW dialog notification / file-open fix

R4 is the manually regression-green PM/GPI baseline (WMCHAR, HANOI, BIO,
HELLO/OPENDLG and Sarien).  R5 added diagnostics only.  The R5 Win32 trace
showed that JIGSAW never reached the Yosemite bitmap code at all: its async
worker called `DosQueryFileInfo` with HFILE 0.

The root cause is in the generic PM dialog bridge.  Win32 multiplexes menu,
button and child-control notifications through `WM_COMMAND`; OS/2 PM uses
`WM_COMMAND` for commands and `WM_CONTROL` for child-control notifications.
The R5 bridge special-cased only listboxes.  Therefore, when JIGSAW's
`CheapWndProc` handled `WM_INITDLG`, its calls to focus/configure the entry
field generated Win32 EDIT `WM_COMMAND` notifications.  Those were incorrectly
sent to the guest as OS/2 `WM_COMMAND` with control id 258.  JIGSAW dismisses
its cheap dialog on any `WM_COMMAND` (only doing DosOpen when the id is OK), so
the dialog was dismissed during initialization and `pli->hf` stayed zero.

R6 fixes this generically:

- Win32 child-control notifications are delivered to the guest as OS/2
  `WM_CONTROL`.
- Listbox select/double-click retains the existing LN_SELECT/LN_ENTER mapping.
- Pushbutton `BN_CLICKED` remains a real OS/2 `WM_COMMAND`, so OK/Cancel still
  drive normal dialog command processing.
- Menu/accelerator commands (`lParam == 0`) remain OS/2 `WM_COMMAND`.
- R5's deep JIGSAW bitmap/GPI diagnostics remain in place.

No JIGSAW application-name check or bitmap-file modification is used.

## Runtime test

Build and run:

    m31f-jigsaw-r6-test.cmd

Choose Load, type/select `YOSEMITE.BMP`, then press OK.  The first success gate
is that the trace must now contain a real `DOSCALLS IO: OPEN ...` followed by a
nonzero HFILE and successful `QUERYFILEINFO`.  If bitmap processing later
fails, preserve `jigsaw-r6-err.txt`; R5's GPI diagnostics will show the next
failure point.
