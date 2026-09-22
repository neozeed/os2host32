# Milestone 30J - EMX DosDevIOCtl device probing

M30J continues the real EMX startup path after M30I's OS/2 memory-suballocation support.
The next deferred import reached by the tiny bound `hi.exe` was DOSCALLS ordinal 284.

## New DOSCALLS API

- ordinal 284: `DosDevIOCtl(HFILE, ULONG category, ULONG function, PVOID parms,
  ULONG parmMax, PULONG parmLen, PVOID data, ULONG dataMax, PULONG dataLen)`

`DosDevIOCtl` is not one operation; it is OS/2's generic device-control gateway.  The
supplied EMX runtime contains 18 call sites.  Its first cluster uses category 01h ASYNC
(serial) requests including function 68h, which queries the receive-queue count.  EMX
uses these calls while classifying HFILEs and probing optional device capabilities.

That means a failed IOCTL is often *normal*.  Before M30J any use of ordinal 284 hit the
loader's deferred-import fatal trap.  M30J instead exports the real nine-argument ABI and
returns ordinary OS/2 capability errors for operations which do not apply to the host
handle.  This allows EMX to take the fallback path it would take on OS/2.

## Implemented behavior

- validates the OS/2 HFILE through the existing Win32 handle table
- traces every call when `OS2_TRACE_IO=1`
- category/function values outside 8-bit OS/2 IOCTL range return
  `ERROR_INVALID_PARAMETER`
- unsupported category/device combinations return `ERROR_INVALID_CATEGORY`
- unsupported functions in a recognized ASYNC category return
  `ERROR_INVALID_FUNCTION`
- category 01h / function 68h (`ASYNC_GETINQUECOUNT`) is implemented for a genuine
  Win32 communications handle using `GetCommState`, `ClearCommError`, and
  `GetCommProperties`
- console, pipe, and disk handles are deliberately *not* misrepresented as serial
  devices; the ASYNC probe fails normally

The remainder of the ASYNC serial configuration operations and EMX's category 76h
extension-device paths stay capability errors until a real test requires them.

With `OS2_TRACE_IO=1`, look for lines such as:

```
M30J DEVIOCTL: hfile=... native=char category=01 function=68 ...
```

For a normal Windows console that call should return a normal nonzero OS/2 capability
error and EMX should continue rather than os2host32 stopping at an unresolved import.

Build with:

```
make clean
make tools compat
```

Then place the known-good bound `hi.exe` and `emx.dll` beside the included unbound
`hi` sidecar and run `m30j-emx-hi-test.cmd`.
