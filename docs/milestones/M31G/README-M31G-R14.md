# M31G R14 - first NEKO runtime-behavior tranche + e.exe DOSCALLS.218

R13 made the untouched OS/2 2.0 GA `NEKO.EXE` import-complete.  The first
Windows run therefore stopped being a loader/API-coverage exercise and exposed
real PM behavior.  R14 fixes the two concrete runtime mismatches shown by that
trace and carries the independent `e.exe` DOSCALLS path one ordinal further.

There are still no executable-name checks or NEKO resource-ID special cases.

## 1. PMWP.203 is a module-loader helper, not an advisory no-op

R5 had only enough evidence to preserve the four-DWORD ABI and zero-success
contract of GA PMWP ordinal 203, so it modelled the call as advisory.  The R13
runtime trace proved that model incomplete: NEKO's subsequent `WinLoadPointer`
calls all used HMODULE zero and therefore could not find the animation frames.

Re-analysis of the original OS/2 2.0 GA `PMWP.DLL` provides stronger evidence.
The successful path of ordinal 203 calls external fixup `DOSCALLS.318`, which is
`DosLoadModule`, structurally as:

    DosLoadModule(fail_name, 0x105, (PCSZ)arg3, (PHMODULE)arg2)

The untouched NEKO call supplies argument 3 as `"NEKO"` and argument 2 as a
writable global HMODULE slot.  It later passes that slot to `WinLoadPointer`.

R14 therefore keeps `PMWPOrdinal203 @203 NONAME` (the trustworthy historical
public symbolic name is still unknown), but now delegates to the existing
DOSCALLS.318 personality and returns its APIRET.  This means normal
`DosLoadModule`/`OS2LIBPATH` semantics select the actual guest DLL.

### Resource-only guest DLLs

A resource DLL does not have to import PMWIN itself.  The guest-module resource
publisher now falls back to the process-level `PMWIN.dll` personality if the
loaded guest DLL has resources but no direct PMWIN import.  This lets ordinary
resource-only OS/2 DLLs publish their RT_* data under their synthetic HMODULE.

## 2. Real WinSubclassWindow bridge for native public controls

NEKO creates a public `WC_STATIC | SS_ICON` window.  Public PM classes are
represented by native Win32 controls, so before R14 they had no `PMCompatWindow`
state and the historical collapsed/no-op `WinSubclassWindow` implementation did
not route messages to NEKO's replacement PFNWP.

The trace then showed native Win32 `WM_TIMER` (`0x0113`) repeatedly arriving at
the STATIC hwnd without ever becoming OS/2 `WM_TIMER` (`0x0024`) in the guest.

R14 adds a generic public-control subclass path:

- allocate compatibility state for a native public control;
- remember its original Win32 `WNDPROC`;
- install `pm_wndproc` with `GWL_WNDPROC`;
- route Win32 messages through the existing native -> OS/2 translation;
- return a guest-callable compatibility PFNWP representing the original public
  control procedure;
- preserve the old collapsed behavior for already-managed/private PM windows,
  so the frozen BIO semantics are not disturbed.

The original Win32 control procedure is still used for native messages that the
compatibility bridge does not consume and receives `WM_NCDESTROY` before the
compatibility state is released.

## 3. WC_STATIC SM_QUERYHANDLE / SM_SETHANDLE

Immediately after subclassing, NEKO sends OS/2 static-control messages
`SM_QUERYHANDLE` (`0x0101`) and `SM_SETHANDLE` (`0x0100`).  The returned original
PFNWP now implements those messages for native STATIC controls using
`STM_GETICON`/`STM_SETICON` (and the bitmap equivalents for SS_BITMAP).

That is important because a subclass procedure normally delegates unhandled
messages to the PFNWP returned by `WinSubclassWindow`; bypassing that model in
`WinSendMsg` would hide the very behavior we need to emulate.

## 4. e.exe: DOSCALLS.218 DosSetFileInfo

The independent `e.exe` path reached DOSCALLS ordinal 218 after R13.
R14 exports:

    DosSetFileInfo @218 NONAME

with the four-argument 32-bit ABI:

    DosSetFileInfo(HFILE, ULONG level, PVOID buffer, ULONG cb)

`FIL_STANDARD` uses the existing synthetic HFILE -> native HANDLE mapping,
converts OS/2 packed create/access/write timestamps, applies them with
`SetFileTime`, and applies the writable attribute bits to the pathname resolved
from the native handle.  File-size/allocation fields remain query fields.  EA
level support is still outside this tranche.

## Regression policy

`make m31g-neko-r14-check` chains the complete frozen M31F -> M31G suite and
then verifies:

- PMWP.203 contains the proven DOSCALLS.318 loader bridge;
- resource-only guest DLLs can publish resources through process PMWIN;
- public controls have a real native subclass bridge;
- static handle messages are implemented by the guest-callable old PFNWP;
- DOSCALLS.218 is present with FIL_STANDARD handle metadata behavior.

The older R5 PMWP checker is intentionally monotonic-safe now: it still checks
the ordinal-only four-DWORD ABI and host-personality routing, but no longer
requires later milestones to preserve the obsolete unconditional-success stub.

## Windows test

The real PMWP.203 semantics load a bare module name (`NEKO`), so the directory
containing `NEKO.DLL` must be on `OS2LIBPATH`.  When launching the executable by
an absolute pathname from another directory, use for example:

    set OS2LIBPATH=C:\cl386-research\os2_2.0\x\OS2\APPS;.

Build:

    make PMWP.dll PMWIN.dll DOSCALLS.dll os2host32.exe

Then run with the PM trace enabled:

    set OS2_PM_TRACE=1
    os2host32 --run C:\cl386-research\os2_2.0\x\OS2\APPS\NEKO.EXE
    os2host32 --run e.exe

Useful NEKO evidence in a successful R14 run will be:

- `GUESTMOD LOAD: NEKO -> ...NEKO.dll ...`
- a non-zero HMODULE in the PMWP.203 trace;
- `GUESTMOD RESOURCES: NEKO ... published=...` if it carries resources;
- `WinLoadPointer OK` instead of the previous HMODULE-zero misses;
- `WinSubclassWindow native` for the STATIC hwnd;
- `WM_TIMER -> guest` / `call_guest enter` for OS/2 message `0x24`.

If `NEKO.DLL` itself proves mixed-mode, stop there and scan that exact module
before adding machinery.  Do not replace it with application-specific host
resources.
