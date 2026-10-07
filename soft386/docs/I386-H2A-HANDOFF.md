# Soft386 I386-H2A handoff — PM ABI boundary reset

## Checkpoint

Base: `os2host32-SOFT386-I386-H1B`

Goal: continue the architecture reset from DOSCALLS into Presentation Manager without adding TelnetPM/Neko-specific behavior.

Prime directive remains:

> Soft386 executes i386 guest code and bridges ABI/representation. Existing OS/2 service DLLs/common code own OS/2 semantics.

## What H2A changes

### 1. PM signatures are now explicitly ABI descriptors

`soft386_pm_bridge.c` now marks descriptor policy separately from the argument representation string.

Current flags:

- `PM_ABI_NONE` — ordinary service call; generic argument/handle marshalling is enough.
- `PM_ABI_CUSTOM` — requires an explicit representation marshaller.
- `PM_ABI_CALLBACK` — contains a guest procedure/callback boundary.
- `PM_ABI_SCHEDULER` — needs jar scheduling/continuation mechanics because guest threads are not native PM threads.

This is deliberately **not** an API-permission or behavior table. It records what the bridge must know to cross the machine/address-space boundary safely.

Trace output now reports the boundary in those terms, for example:

```
soft386: PM ABI PMWIN.dll.920 WinSendMsg class=scalar/handle argc=4 -> native service
```

A missing ABI descriptor is diagnosed as such and the native call is not guessed.

### 2. Control-message behavior moved out of Soft386

The old `control_message()` switch mixed representation facts with names/comments for button, edit, listbox, menu, scrollbar and slider operations.

H2A replaces it with `message_abi[]` + `marshal_message()`.

That table knows only representation classes:

- scalar MPARAMs may cross unchanged;
- `SM_SETHANDLE` translates an HPOINTER argument;
- `SM_QUERYHANDLE` translates an HPOINTER return value.

It does **not** implement what BM/EM/LM/MM/SBM/SLM messages mean. That behavior remains in PMWIN/PMCTLS.

The PM boundary regression now proves this explicitly: known scalar `WinSendMsg` and `WinSendDlgItemMsg` calls reach the provider and return the provider result; an unclassified message is rejected before the provider is called.

### 3. Diagnostics describe ABI failure, not unsupported PM behavior

Rejected calls now produce `PM ABI reject ... class=...` diagnostics. Missing descriptors produce `PM ABI descriptor missing ... call not attempted`.

This distinction matters for the next applications: a failure should tell us whether we need a pointer/structure/message schema, not tempt us to recreate PM behavior in the CPU jar.

## What deliberately remains from R5D/H1B

H2A does **not** delete working representation/scheduling machinery merely because it lives in the PM bridge.

Still retained:

- typed guest <-> native PM handle tokens;
- guest string/buffer/array/structure copy-in/copy-out;
- native -> guest procedure callback veneers;
- subclass old-procedure veneers;
- dialog callback/re-entry support;
- bitmap/PS/DC/region representation bookkeeping;
- QMSG integrity/token records;
- nonblocking native `WinPeekMsg` integration;
- guest PM-owner scheduling/yield state.

These are machine/ABI differences and remain legitimate Soft386 responsibilities.

## Important remaining architectural debt

### Jar-owned posted-message queue is still present

R5D's `new_message()`, `post_message()`, `get_message()` and `dispatch_message()` path still contains a private queue for `WinPostMsg` / `WinPostQueueMsg` and merges it with native `WinPeekMsg` traffic.

This is the largest remaining PM-semantic-looking block.

It was **not** ripped out in H2A because the current native PMWIN `WinGetMsg` blocks the host thread, while Soft386 must yield a guest PM thread and continue scheduling other guest threads. R5D therefore uses nonblocking `WinPeekMsg` and a continuation-like `THREAD_WAIT_PM` state.

The next cleanup must preserve that scheduling property while moving queue semantics back into PMWIN/common code. The correct fix is not simply to call blocking native `WinGetMsg` from the single Soft386 execution host.

### `WinSendMsg` still has a direct guest-procedure fast path

For jar-owned windows, R5D may invoke the guest procedure directly rather than enter PMWIN and let PMWIN call the callback veneer. This should be audited next. Same-thread calls should preferably flow through PMWIN once callback/scheduler behavior is proven equivalent; cross-guest-thread synchronous sends may still require jar continuation machinery.

### Message ABI coverage is intentionally explicit

Unknown `MPARAM` layouts still fail closed. H2A does not turn arbitrary message values into scalar calls because many PM messages carry pointers or handles. New coverage should be added as representation schemas, not as behavior implementations.

## Validation completed in this environment

Passed:

- `make -C soft386 check-quick`
  - hello LE
  - guest threading / DosWaitThread
  - mutex/event scheduling
  - guest memory semantics
  - DOSCALLS bridge boundary test
  - system DLL boundary test
- standalone `pm-bridge-check`
- `tests/test_r5.py`
  - DLL imports
  - PM callback runtime
  - PM marshal boundaries
  - callback failure unwind

Existing Tiny386 compiler warnings remain unchanged.

No Windows display-backed TelnetPM/Neko acceptance was attempted in this checkpoint. That remains intentionally deferred until the PM boundary is cleaner.

## Recommended next milestone: I386-H2B

1. Move PM posted-message ownership out of the jar without blocking the single execution host.
   - Prefer a native/common PM queue primitive or service-thread/continuation seam.
   - Soft386 should own only guest-thread wait/resume and QMSG representation conversion.
2. Route `WinPostMsg`/`WinPostQueueMsg` to PMWIN semantics rather than `post_message()`.
3. Audit `WinSendMsg` direct guest dispatch.
   - Same-thread native PM path first.
   - Preserve explicit scheduler continuation only where cross-thread guest execution requires it.
4. Keep `WinGetMsg` scheduling nonblocking from the jar's perspective.
5. Expand message schemas only when traces prove a pointer/handle representation is needed.
6. Re-run R5 PM regressions before TelnetPM/Neko.

## Acceptance rule

Do not add an application category.

For every failure classify it as one of:

- CPU execution;
- loader/import resolution;
- missing ABI descriptor;
- missing buffer/structure/message marshaller;
- handle translation;
- callback/re-entry;
- scheduler/continuation boundary;
- native service/common-backend bug.

TelnetPM and Neko remain acceptance tests, not architecture inputs.
