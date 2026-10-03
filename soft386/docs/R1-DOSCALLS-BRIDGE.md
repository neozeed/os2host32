# Soft386 R1 — DOSCALLS ownership and native bridge

R1 separates the 32-bit OS/2 personality into two intentionally different
kinds of state.

## 1. Jar kernel state

Anything whose identity or blocking semantics are visible to the virtual OS/2
process remains authoritative inside Soft386 guest/kernel state:

* virtual address allocations and OS/2-visible protection flags
* OS/2 virtual threads and saved Tiny386 CPU contexts
* PIB/TIB state and per-thread FS descriptors
* thread waits and sleeps
* event semaphores
* mutex semaphores, ownership, recursion and abandoned-owner handling
* process exit state
* guest module addresses/import veneers

These objects must not be replaced by Win32 `VirtualAlloc`, Win32 thread IDs,
Win32 mutex handles, or the corresponding objects in native `DOSCALLS.dll`.
Doing so would create a second kernel beside the virtual scheduler.

R1 proves this rule with executable guest fixtures for threads, event/mutex
blocking, and `DosAllocMem`/`DosSetMem`/`DosQueryMem`/`DosFreeMem`.

## 2. Native service state

Operations whose durable state is inherently a host service may be delegated
to the existing native 32-bit `DOSCALLS.dll` on Win32.  R1 starts with the
file/path/HFILE/HDIR family plus simple host timers and app-type queries.

The key rule is ownership continuity.  If `DosOpen` returns an HFILE from the
native DLL, every operation on that HFILE must stay in the same native DLL
session.  R1 therefore routes `DosRead`, `DosWrite`, `DosClose`,
`DosSetFilePtr`, `DosQueryHType`, `DosDupHandle`, file-information and related
handle calls through the bridge whenever it is active.

This is not just an optimization.  An HFILE returned by native `DOSCALLS.dll`
is an opaque OS/2 personality handle owned by that DLL's common Win32 session;
it is not a CRT fd and is not meaningful to Soft386's old R0 stdout fallback.

## 3. Pointer marshalling

A guest linear address is never passed to native `DOSCALLS.dll` as a C pointer.

For each bridged API the host gate has explicit argument knowledge:

1. scalar arguments are copied from the guest cdecl stack;
2. input strings/buffers are range-checked and copied to bounded host storage;
3. native `DOSCALLS.dll` is called with real host pointers;
4. output buffers/scalars are copied back only to validated guest ranges;
5. native pointers are never exposed to the guest.

R1 caps temporary buffers at 64 MiB and strings at 4096 bytes.  Calls with an
unsupported pointer shape (for example the initial `DosOpen` EA path) fail with
`ERROR_INVALID_PARAMETER` instead of forwarding an unsafe address.

`tests/bridge-check.c` injects a fake DOSCALLS provider and proves copy-in,
copy-out, and opaque HFILE propagation for `DosOpen`, `DosWrite`, `DosRead`,
`DosQueryCurrentDisk` and `DosQueryHType` without requiring Windows.

## 4. Native bridge R1 tranche

When `DOSCALLS.dll` is loaded, the following ordinals are eligible for native
marshalling:

| Ordinal | API | Ownership reason |
|---:|---|---|
| 110 | DosForceDelete | host filesystem |
| 209 | DosSetMaxFH | native HFILE namespace |
| 218 | DosSetFileInfo | native HFILE + host filesystem |
| 219 | DosSetPathInfo | host filesystem |
| 220 | DosSetDefaultDisk | host filesystem session |
| 221 | DosSetFHState | native HFILE |
| 223 | DosQueryPathInfo | host filesystem |
| 224 | DosQueryHType | native HFILE when bridge is active |
| 226 | DosDeleteDir | host filesystem |
| 239 | DosCreatePipe | native HFILE namespace |
| 254 | DosResetBuffer | native HFILE |
| 255 | DosSetCurrentDir | host filesystem session |
| 256 | DosSetFilePtr | native HFILE |
| 257 | DosClose | native HFILE |
| 258 | DosCopy | host filesystem |
| 259 | DosDelete | host filesystem |
| 260 | DosDupHandle | native HFILE |
| 263 | DosFindClose | native HDIR |
| 264 | DosFindFirst | native HDIR + copied find buffer |
| 265 | DosFindNext | native HDIR + copied find buffer |
| 270 | DosCreateDir | host filesystem; EA pointer deferred |
| 271 | DosMove | host filesystem |
| 272 | DosSetFileSize | native HFILE |
| 273 | DosOpen | host filesystem; creates native HFILE |
| 274 | DosQueryCurrentDir | host filesystem session |
| 275 | DosQueryCurrentDisk | host filesystem session |
| 276 | DosQueryFHState | native HFILE |
| 278 | DosQueryFSInfo | host filesystem |
| 279 | DosQueryFileInfo | native HFILE |
| 281 | DosRead | native HFILE; buffer copied back |
| 282 | DosWrite | native HFILE; buffer copied in |
| 323 | DosQueryAppType | host file inspection |
| 362 | DosTmrQueryFreq | host timer service |
| 363 | DosTmrQueryTime | host timer service |
| 382 | DosSetRelMaxFH | native HFILE table |

If the bridge is absent, the already implemented R0 local calls remain
available where applicable (for example the historical `hi.exe` surface).
New host-filesystem calls simply remain unsupported rather than silently
passing guest pointers to host code.

## 5. Why the common catalog is not the final Soft386 router

`common/api/os2_api_catalog.inc` describes the ownership/routes used by the
native os2host32 personality.  It is an excellent inventory, but a catalog
entry marked `NATIVE_ONLY` does **not** automatically mean it is safe for
Soft386 to call the native DLL.

Examples:

* `DosQueryMem` is `NATIVE_ONLY` in the native catalog because native os2host32
  queries Win32 virtual memory.  In Soft386 it must query **guest** allocations.
* `DosLoadModule`/`DosQueryProcAddr` operate on guest LE/LX module addresses and
  must eventually be owned by the jar loader.
* signal/exception/selector APIs describe guest CPU/process state and cannot
  return native pointers/selectors.
* resource APIs use guest module handles and eventually return guest-visible
  addresses; they belong with the guest module loader even though native
  os2host32 implements them with Win32-backed mechanics.

R1 therefore treats the catalog as the API inventory and documents a separate
Soft386 ownership policy.

## 6. Deliberately jar-owned or hybrid families

The following remain out of the native bridge:

* 229/234/311/312/349 — scheduler, exit, thread/TIB operations
* 299/304/305/306 and 344-347 — guest memory/suballocation
* 324-336 — guest event/mutex semaphores
* 318-322, 352/353/572 — guest module/resource namespace
* 354/355 and signal/vector APIs — guest exception/signal state
* 425/426 — guest selector/address conversion
* NLS/process-codepage APIs — shared OS/2 process state
* `DosScanEnv` — must inspect the guest environment, not the host environment

`DosExecPgm`/`DosWaitChild` are future **hybrid** APIs: native process creation
may be a backend mechanism, but guest PID/wait semantics must remain under the
Soft386 process scheduler.  `DosDevIOCtl` is similarly a good future native
marshaller for DLL-owned HFILEs but needs its two variable in/out buffer
contracts handled explicitly first.
