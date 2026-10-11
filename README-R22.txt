R22: jar list-box marshalling and modal scheduler hook
Apply over os2host32-2bc833b plus R21. Source overlay, not compiled binaries.

Build from repository root with your existing MinGW environment:
  make PMWIN.dll
  make -C soft386 win32
Install BOTH PMWIN.dll and soft386/soft386_os2.exe in their runtime locations.
Retain R21 DOSCALLS.dll. WinTown, resources and PMGPI do not need rebuilding.

Confirmed browser cause: the jar bridge lacked LM_INSERTITEM (0x161) and
LM_QUERYITEMTEXT (0x168). Unknown messages were rejected before PMWIN.
This adds bounded guest string copy-in and bounded text copy-out. Guest
pointers never pass directly into PMWIN. Selection now has the text retrieval
needed for double-click file/folder selection.

Telnet finding: the jar already looks up OS2PM_SetSchedulerHook, but the supplied
PMWIN had neither that export nor the modal-loop callback. Its GetMessage
blocked other emulated guest threads. Restore the named hook and bounded guest
scheduler calls in WinDlgBox, using a 1ms message wait when idle. Without a
hook native execution keeps the original GetMessage loop. WM_QUIT is reposted,
and dismissal during a scheduler quantum is checked before reading messages.
This matches the observed Cancel-then-connect symptom; live confirmation needed.

Validation: real bridge regression suite passes, including new string input,
text output, untouched output tail and invalid guest address rejection tests.
PMWIN change was source reviewed only: no Windows compiler/runtime available.

Test browser listing and double-click selection, then Telnet connect without
Cancel. If Telnet still stalls, run your usual soft386 command with:
  --trace-native --trace-sched
alongside OS2_PM_TRACE=1 and capture stdout/stderr. This exposes the scheduler
and network bridge activity absent from the supplied PM-only log.

The ZIP includes source overlay and changes.patch (alternative application).
