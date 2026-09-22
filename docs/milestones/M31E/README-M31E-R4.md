# M31E R4 — OPENDLG listbox redraw + INIT-only cleanup

R4 fixes two runtime issues exposed by the real 1990 OPENDLG.DLL sample.

1. `WinEnableWindowUpdate(FALSE)` is emulated with `WM_SETREDRAW(FALSE)`, but OS/2 documents that a later `WinShowWindow(hwnd, TRUE)` presents/redraws the accumulated changes. R4 remembers the suppressed-update state and re-enables/redraws on `WinShowWindow(TRUE)`. OPENDLG depends on this exact sequence while bulk-populating its Files and Directories list boxes.
2. INITINSTANCE-only guest DLLs have no termination callback. R3 correctly skipped the callback but failed to mark termination complete, causing `OS2HostModuleTerminateAll()` to select OPENDLG forever after HELLO received `WM_QUIT`. R4 marks such modules `term_called` and emits `GUESTMOD TERMSKIP`.
