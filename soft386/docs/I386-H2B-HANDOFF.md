# Soft386 I386-H2B Handoff — Provider-First Service Resolution

## Status

**SOURCE/REGRESSION CHECKPOINT. Windows live acceptance pending.**

Base: `os2host32-SOFT386-I386-H2A`.

The Windows evidence supplied immediately before H2B came from a binary whose
banner was `Soft386 OS/2 R4 - Tiny386 + 80387 + process vessels + jar kernel`.
Those runs are the acceptance evidence that motivated H2B; they are not H2B
runtime results. A valid H2B retest must first show:

```
Soft386 OS/2 I386-H2B - provider-first service resolution
```

## Architectural rule

The native service provider decides whether an OS/2 export exists. Soft386 ABI
metadata decides only whether the i386 guest representation can safely cross
to that export.

There is no legitimate intermediate policy question of the form "does
Soft386 support MODULE.ordinal?"

## 1. Old DOSCALLS.286 rejection path

In the old/R4 live run the LE import was recognized and a guest veneer was
created for `DOSCALLS.286`, but runtime dispatch later printed
`unsupported DOSCALLS.286`.

H2A had already added a DosBeep ABI descriptor, but it still retained the
architectural coupling between native bridge admission and the ABI descriptor
registry through `soft386_doscalls_bridge_ordinal()`.

## 2. H2B DOSCALLS change

H2B adds `soft386_doscalls_bridge_export()` as an independent provider query.
`dispatch_doscalls()` now asks the loaded DOSCALLS provider whether the ordinal
is exported independently of ABI metadata.

For a native exported call:

```
provider export -> ABI descriptor -> scalar/marshalled crossing -> provider
```

For an exported ordinal with no descriptor:

```
soft386: service DOSCALLS.N export resolved; ABI descriptor missing; call not attempted
```

For a missing provider export:

```
soft386: service DOSCALLS.N native export missing
```

The old generic `unsupported DOSCALLS.N` terminal diagnostic has been removed
from this path.

### DosBeep

No DosBeep semantic implementation was added. Ordinal 286 remains a two-scalar
ABI descriptor with the generic `MAY_BLOCK` execution flag. It uses the same
generic scalar caller as the other scalar DOSCALLS entries.

## 3. QUECALLS provider/ABI split

Previously the queue bridge began with an ordinal membership test for
9/14/15/16, conflating marshaller coverage with service existence.

H2B now resolves the native QUECALLS export first through
`soft386_queue_export()`.

* Missing native export -> `native export missing`.
* Native export exists but no queue ABI marshaller ->
  `export resolved; ABI descriptor missing; call not attempted`.
* Known queue ABI -> existing token/pointer/wait marshalling proceeds.

A regression provider now deliberately exports ordinal 99 while no Soft386
marshaller exists for it. The test proves export existence and ABI coverage are
independent and that the provider function is not called blindly.

## 4. PM provider/ABI split

Previously `soft386_pm_dispatch()` tested `signature(module, ordinal)` before
opening/resolving the provider. Thus absence from the PM signature table could
act like absence of the API itself.

H2B reverses that ownership:

1. open the appropriate native PM provider;
2. resolve the ordinal through the provider;
3. only then inspect the PM ABI signature/marshaller;
4. call only when the representation crossing is described safely.

A PM regression provider deliberately exports ordinal 999 with no PM signature.
The test proves H2B detects the real export, reports missing ABI metadata, and
does not invoke it.

Existing PM signatures therefore describe **ABI shape**, not API availability.

The H2A message-ABI reset remains intact: message tables describe scalar,
handle, pointer and callback representation only; PMWIN/PMCTLS own control
semantics.

## 5. Module routing

H2B removes ordinal allowlist semantics from DOSCALLS, QUECALLS and PM dispatch.
The existing `system_module_id()` table still selects which bridge family owns
a module name (DOSCALLS, PMWIN/PMGPI/PMCTLS, QUECALLS, VIO/KBD/etc.). It no
longer means that a particular ordinal is supported.

This module-family routing table is **remaining architectural debt**. A future
cleanup can replace the fixed family-name routing with a service-provider
registry/dynamic hostcall slot mechanism. Do not solve that by adding more
application/module exceptions.

## 6. ABI machinery intentionally retained

Still valid Soft386 ownership:

* guest pointer validation and translation;
* copy-in/copy-out buffers and structures;
* nested pointer graphs;
* native-handle <-> guest-token translation;
* callback/re-entry veneers;
* guest thread wait/resume continuations;
* import veneer execution;
* explicit rejection where no safe ABI crossing exists.

Guest-local DOSCALLS operations that fundamentally affect the jar's address
space/process/CPU contexts remain local (memory, threads, semaphores, process
exit/module mapping, etc.).

## 7. Regression results in this environment

Passed:

* `make check-quick`
  * hello LE/LX execution
  * guest threading / DosWaitThread
  * mutex/event scheduling
  * jar memory semantics
  * DOSCALLS bridge marshalling
  * system-DLL bridge marshalling
* `pm-bridge-check`
  * including provider-export / missing-ABI separation
* `queue-bridge-check`
  * including provider-export / missing-ABI separation
* `bridge-check`
  * including generic scalar DosBeep descriptor/caller

The environment does not provide a 32-bit MinGW cross compiler, so no Windows
H2B executable is claimed here.

## 8. Required Windows H2B acceptance

First verify the banner is H2B, not R4/H2A.

### Gate A — controls

```
soft386_os2.exe --run demos\phoon\phoon.exe
soft386_os2.exe --run demos\nlsinfo.exe
```

Both should retain their current successful behavior.

### Gate B — Beep

```
set OS2_SOFT386_TRACE=1
soft386_os2.exe --run demos\beep.exe
```

Required:

* native DOSCALLS.286 export resolves;
* three calls use generic scalar forwarding;
* no `unsupported DOSCALLS.286`;
* no DosBeep-specific Soft386 semantic implementation.

### Gate C — QUECALLS

```
soft386_os2.exe --run demos\sarien\sarienle.exe
```

Required for H2B:

* `QUECALLS.9` is not rejected because of a Soft386 ordinal/module allowlist;
* if exported, it reaches the queue ABI marshaller;
* any later Sarien failure is classified separately.

### Gate D — PMWIN

```
soft386_os2.exe --run demos\WMCHAR.EXE
```

Required for H2B:

* PMWIN.834 reaches the native PMWIN provider/export check;
* if a safe ABI descriptor exists, it proceeds through it;
* otherwise the diagnostic must explicitly say the export exists but ABI
  marshalling is missing;
* no `unsupported import module PMWIN.834` policy rejection.

## 9. Explicitly out of scope

Do not mix these into H2B:

* BIO/JIGSAW/HANOI `overlapping main objects` loader work;
* TelnetPM fixes;
* Neko fixes;
* new PM control semantics;
* 16-bit protected mode.

## Recommended next decision

Do not call H2B live-proven until the Windows Beep/QUECALLS/PMWIN gates above
have run with the H2B banner.

If those pass, the next architecture milestone should address the remaining
fixed module-family routing and the H2A posted-message queue ownership debt,
without returning to API/application allowlists.
