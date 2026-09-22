# OS2HOST32 Milestone 30A checkpoint — EMX mixed-mode bridge

This is an intermediate Milestone 30 checkpoint taken while bringing up the
supplied EMX runtime (`emx.dll`) and the supplied EMX-linked Dhrystone sample.
It is intentionally conservative: the loader recognizes the exact EMX DLL
profile used for this bring-up rather than claiming generic EMX compatibility.

## What is implemented in this checkpoint

* Exact-profile recognition for the supplied 81,982-byte LX `emx.dll`.
* Acceptance of that DLL despite its small 16-bit object and selector/far
  pointer fixups, but only after the complete known fixup profile matches.
* Native replacement of the EMX mixed-mode helpers:
  * `EMX_32TO16` becomes a flat-pointer identity helper.
  * `EMX_16TO32` becomes a flat-pointer identity helper.
  * `EMX_THUNK1` is redirected to a generated 32-bit dispatcher.
* Nine proven far16 API wrapper shapes are described and dispatched natively:
  * `DOSCALLS.14` — `DosSetSigHandler`
  * `DOSCALLS.15` — `DosFlagProcess`
  * `KBDCALLS.4` — `KbdCharIn`
  * `KBDCALLS.9` — `KbdStringIn`
  * `KBDCALLS.10` — `KbdGetStatus`
  * `KBDCALLS.11` — `KbdSetStatus`
  * `KBDCALLS.13` — `KbdFlushBuffer`
  * `KBDCALLS.22` — `KbdPeek`
  * `VIOCALLS.21` — `VioGetMode`
* Argument widths for those wrappers have been checked against the wrapper
  instruction sequences.  In particular:
  * `DosSetSigHandler`: `2,2,4,4,4`
  * `DosFlagProcess`: `2,2,2,2`
* The remaining tenth `PTR16:16` record is part of EMX's tiny 16-bit startup
  shim (`DOSCALLS.14`), not another 32-bit generic wrapper.
* OS/2 `FS`/TIB reads used during EMX startup are currently virtualized as
  main-thread TID 1.  This is deliberately a single-thread bring-up model.
* Unresolved imports in the recognized EMX image can be deferred to small
  trap-on-use stubs.  If reached, they print the exact module/ordinal that is
  needed next instead of failing the DLL at load time.
* Main guest image is represented by synthetic HMTE/HMODULE 1 in the PIB.
* Added host/module plumbing for `DosQueryModuleName`.
* Added initial implementations needed by EMX startup:
  * `DosExitList`
  * `DosQueryMem`
  * `DosQueryModuleName`
  * keyboard status/peek support (`KbdGetStatus`, `KbdSetStatus`, `KbdPeek`)
  * exports already added during this checkpoint include the far16 bridge
    targets `DosSetSigHandler`, `DosFlagProcess`, and `DosSelToFlat`.

## Scanner result

With this checkpoint's host scanner, the supplied EMX DLL ends with:

    Execution model : contains 16-bit LE/LX objects/selectors
    M30 EMX bridge   : recognized exact generic 32/16 thunk profile
    Direct host path: supported via native EMX generic bridge

This is the key change from M29P1, which rejected the DLL as requiring
unsupported mixed-mode machinery.

## Important status / limitations

This is **not yet a completed M30 release**.  It is the current source
checkpoint before the first successful Dhrystone execution through EMX.

The Linux-side scanner builds and recognizes the supplied DLL.  A Win32/i686
build and runtime test are still required for the generated machine-code bridge.
The current development container does not have the i686 MinGW cross compiler,
so the generated Win32 path has not been executed here.

The exact-profile check is intentional.  Once the supplied specimen executes,
we can generalize recognition around actual EMX structures rather than silently
accepting unrelated mixed-mode LX images.

## Expected next test on Windows

Build the normal compatibility set and put the supplied `emx.dll` next to the
EMX-linked `dhyrstone.exe`, then run through `os2host32` / `cmd32os2` as before.
The useful first failure is no longer a mixed-mode rejection: if startup reaches
an unimplemented deferred import, M30 prints the precise module and ordinal so
that API can be added deliberately.

