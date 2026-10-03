# DOSCALLS routing map — Soft386 R1

This map is specific to the Soft386 execution backend.  It intentionally does
not equate the generic native catalog route with the Soft386 ownership domain.

## Jar kernel — implemented now

* 229 `DosSleep`
* 234 `DosExit`
* 299 `DosAllocMem`
* 304 `DosFreeMem`
* 305 `DosSetMem`
* 306 `DosQueryMem`
* 311 `DosCreateThread`
* 312 `DosGetInfoBlocks`
* 324-330 event semaphore family
* 331-336 mutex semaphore family
* 349 `DosWaitThread`

Shared/common semantics still used locally:

* 230 `DosGetDateTime`
* 348 `DosQuerySysInfo`
* 224/256/282 retain an R0 local fallback when the native HFILE bridge is off

## Native DOSCALLS.dll marshaller — implemented R1

110, 209, 218, 219, 220, 221, 223, 224, 226, 239, 254, 255, 256, 257,
258, 259, 260, 263, 264, 265, 270, 271, 272, 273, 274, 275, 276, 278,
279, 281, 282, 323, 362, 363, 382.

These APIs use a single native DLL session so its HFILE/HDIR values remain
coherent across calls.

## Jar kernel — next logical implementations

* 232 `DosEnterCritSec` (and the matching ABI when added to the catalog)
* 236 `DosSetPriority` — virtual thread scheduling state
* 296 `DosExitList` — process-owned callbacks
* 344-347 suballocation APIs — guest heap regions
* 318-322 module loading/querying — extend the current LE/LX loader
* 352/353/572 resources — resolve against guest modules, copy resource data to
  guest RAM
* signal/vector/exception APIs — guest process/thread state
* selector helpers — guest GDT/LDT state when the later 16-bit phase starts

## Hybrid — future

* 280 `DosWaitChild` / 283 `DosExecPgm`: host process mechanics with jar-owned
  PID/wait state
* 284 `DosDevIOCtl`: native HFILE but two marshalled variable in/out areas
* 286 `DosBeep`: host audio mechanism without blocking all virtual threads
* 228 `DosSearchPath`: host filesystem search, but environment-name mode must
  consult the guest environment
* 227 `DosScanEnv`: guest environment only

## 16-bit deferred

All C/386 16:16/far-pointer migration, DOS16 APIs, VIO/KBD far bridges and
selector conversion remain outside R1.  The current R1 host gate is deliberately
32-bit cdecl only.
