# Soft386 I386-H1A — Generic scalar service dispatch reset

Base: `os2host32-SOFT386-R5D.zip`.
Architecture spec: `SOFT386-I386-HELPER-PLAN.md` supplied 2026-10-06.

## Purpose

H1A is the first implementation step of the i386-helper reset.  Soft386 is a
machine/ABI jar, not an alternate OS/2 personality.  Service semantics stay in
the established DLL/common layer.  The jar owns only CPU/address-space/process
realities plus representation changes required to cross that boundary.

This tranche deliberately does **not** attempt another TELNETPM or NEKO fix.

## Main change: scalar calls are ABI metadata, not semantic cases

R5D routed DOSCALLS.286 DosBeep through a private `case 286` in
`dispatch_doscalls()` and then disguised it as network module 2 solely to reuse
the network worker.  H1A removes that path completely.

`soft386_doscalls_bridge.c` now has ABI descriptors whose fields describe only:

- ordinal;
- argument count;
- scalar representation;
- whether the native call may block;
- diagnostic name.

The common scalar caller supports 0..6 32-bit scalar arguments.  No DosBeep
behavior exists in Soft386.  DOSCALLS.286 is currently the first proof entry:

    { 286, 2, SCALAR | MAY_BLOCK, "DosBeep" }

The existing DOSCALLS.DLL export supplies the semantics.

## Blocking is execution policy, not API semantics

DosBeep must not stall every guest thread while the native implementation is
synchronous.  H1A therefore adds a generic asynchronous scalar invocation path
to the DOSCALLS ABI bridge.  It snapshots scalar arguments before starting a
worker; worker threads never read/write guest RAM or CPU state.  The owner guest
thread retries the hostcall while other jar threads remain schedulable.

This is reusable machine-boundary plumbing.  Future scalar calls that may block
can opt into the same flag without acquiring an API-specific dispatcher case.

The previous fake `module==2` / `beep_proc` path was removed from
`soft386_net_bridge.*`; the network bridge is network-only again.

## Ownership preserved

The existing jar-owned paths remain unchanged for operations whose truth lives
inside the guest process/machine, including guest memory, Tiny386 threads and
scheduler state, guest semaphores, guest module mapping, PIB/TIB state and
callback execution state.

No PM semantics were moved in this tranche.  PM cleanup is H2; H1A establishes
the dispatch pattern that PM should converge on: descriptor -> marshal ->
existing DLL -> unmarshal.

## Tests

New bridge checks prove:

1. DOSCALLS.286 is admitted by ABI metadata.
2. It is classified as two scalar arguments and MAY_BLOCK.
3. The generic synchronous scalar engine calls an injected ordinal-286 export.
4. The generic asynchronous scalar engine snapshots and executes the same call
   and completes through the owner-slot retry path.

`make -C soft386 check-quick` passes locally, including:

- hello LE;
- guest threads / DosWaitThread;
- mutex/event scheduler fixture;
- guest memory fixture;
- DOSCALLS bridge checks;
- system DLL marshalling checks.

A full `make check` was started.  R2C, R3, R4 and R5 regressions completed and
passed before the execution environment terminated the long command during the
R5A phase.  No failure had been reported at termination.  Therefore H1A does
**not** claim a complete full-suite pass here.

A 32-bit MinGW compiler was not installed in the build environment, so the
Win32 target was not rebuilt here.  Windows runtime remains pending user build
and test.

## Next H1 steps

Do not add application fixes yet.

1. Inventory every DOSCALLS ordinal against the canonical API catalog and mark
   ownership separately from ABI representation.
2. Expand the scalar descriptor class for APIs that truly contain only scalar /
   opaque values.
3. Convert existing pointer-bearing cases into explicit marshaller descriptors
   rather than semantic admission switches.
4. Apply the same model to VIO/KBD/MOU/QUE and then PM.
5. For PM, treat signature tables as marshaller metadata, **not an API whitelist**.
6. Identify and retire duplicated PM message/control semantics only after the
   established DLL path is proven to own them correctly.

TELNETPM and NEKO remain acceptance tests, not architectural categories.
