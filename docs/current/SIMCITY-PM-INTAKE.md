# SimCity PM import intake

The supplied `simcity.exe` is a flat 32-bit LE application, suitable for the
native Win32 direct-host path.  It has two objects, 916 internal fixup records
(2,088 sites), and 57 external call sites.

Its DOSCALLS imports are all already exported by the personality.  Its PMGPI
imports are likewise already exported by PMGPI.  Before this intake the only
unresolved PMWIN import was ordinal 885:

| Module/ordinal | OS/2 API | Native implementation |
| --- | --- | --- |
| `PMWIN.885` | `WinStopTimer(HAB, HWND, USHORT)` | `KillTimer` |

`PMWIN.884` (`WinStartTimer`) was already present and maps to `SetTimer`; the
new export completes the timer pair with matching per-window timer identity.

Consequently, the loader should now resolve all 57 external sites.  The next
runtime boundary is application execution: window creation, its event loop,
and the GPI drawing pattern are already within the existing PMWIN/PMGPI scope,
but need a Windows-host trace to verify this specific program's assumptions.
