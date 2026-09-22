# M31G R12 - PMWIN.884 + DOSCALLS.219

R12 follows two independent runtime-proven boundaries.

* NEKO reaches `PMWIN.884 = WinStartTimer(HAB, HWND, ULONG idTimer, ULONG timeout)`.
  The compatibility layer maps it to a Win32 window timer and translates native
  `WM_TIMER` into OS/2 `WM_TIMER` (0x0024) for compatibility window/dialog procs.
* `e.exe` reaches `DOSCALLS.219 = DosSetPathInfo`.
  R12 implements the 32-bit five-argument API for `FIL_STANDARD`, preserving the
  OS/2 zero-date/time "leave unchanged" convention and mapping timestamps plus
  file attributes to Win32 metadata.

No executable-name checks are present. Future timer/path-info levels remain driven
by actual runtime boundaries.
