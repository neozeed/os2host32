# Soft386 I386-H1B handoff

H1B continues the architecture reset from H1A.

## Change

The following native DOSCALLS APIs are now generic scalar/opaque descriptors rather than individual switch bodies:

- 209 DosSetMaxFH
- 220 DosSetDefaultDisk
- 221 DosSetFHState
- 254 DosResetBuffer
- 257 DosClose
- 263 DosFindClose
- 272 DosSetFileSize
- 286 DosBeep (`MAY_BLOCK`)

`call_scalar()` is the only implementation of their host call ABI. DosBeep retains the generic asynchronous scheduling flag introduced in H1A. No API semantics were moved into Soft386.

Pointer-bearing native DOSCALLS calls remain explicit marshallers. Guest-local memory/thread/synchronization/module state remains in the jar.

H1B also removes the separate native ordinal whitelist and argument-count switch. A single `dos_abi[]` registry now owns native admission, argument count and ABI class. Thus the remaining ordinal switch in `soft386_doscalls_bridge_dispatch()` contains only representation-specific marshallers, not admission policy.

## Validation in this environment

PASS:
- `bridge-check`
- `system-bridge-check`
- `pm-bridge-check`
- `queue-bridge-check`
- `make check-quick`
- R2C regression
- R3 regression
- R4 regression

The Linux/POSIX build emits pre-existing Tiny386 warnings during some runtime-test builds; no H1B failure was observed. Native Win32 DLL runtime testing remains for the user build environment.

## Next step

Apply the same separation to the PM bridge: signature metadata must describe marshalling, handle conversion and callback requirements, not act as a second PM implementation or an application-driven admission whitelist. Preserve the existing callback/typed-handle machinery while auditing private message/control semantics for routing back to PMWIN/PMGPI/PMCTLS.
