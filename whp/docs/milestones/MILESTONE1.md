# Milestone 1 — SarienPM under WHP

Date: 20 September 2026. Baseline: R12 fix 2. Status: demonstrated on the user's Windows hardware.

An existing OS/2 LX Sarien executable runs using a Win64 WHP host, without converting the guest executable to PE or booting an OS/2 installation. Guest x86 instructions execute inside the WHP partition; the host supplies the implemented OS/2 services. This is a focused OS/2 personality, not a complete OS/2 kernel or full PM implementation.

## Confirmed progression

| Phase | User-reported result |
|---|---|
| Initial loader/runtime | Untouched LE hello world; startup, memory and console output |
| R3 | Guest DLLs, cooperative threads and events |
| R4 | LE/LX loading and scan/check support |
| R5 | fileio1 PASS; Infocom save/restore |
| R6 | find1 PASS |
| R7 | Thread/process information; 14 completed torture runs of 30 workers × 1000 rounds |
| R8 | sleep1, mutex1, queue1 PASS; recycle1 PASS with 1000 threads in one process |
| R9 | callback1 and callbackdll PASS; C/386 + LX linker builds without reported issues |
| R10 | pmhello PASS: 5 paints, 3 character messages |
| R11 | gpi1 PASS: 3 paints; bitmap readback, copy and cleanup |
| R12 | sarprep1 PASS: 40 worker frames, 85 ticks, 1 paint, 7 keys |
| R12 fixes 1–2 | SQ2 gameplay, audible beeps, working restore, corrected full geometry, clean exit |

The final supplied trace has 6109 lines and includes:

```
v2: Sarien geometry: client=320x200 native frame=336x239
v2: guest process exited rc=0
```

A search of that trace found no unsupported-call or fault messages. The screenshot confirms the status bar and right edge are visible. This records the tested session, not a claim that every game path or OS/2 API is covered. The trace is retained under validation/sarien-milestone1-windows-trace.txt.

## Two Sarien compatibility fixes

**Directory detection.** The actual executable's helper leaves the DosFindFirst input count uninitialized, searches *.* instead of its fname argument, ignores the API result, and derives a Boolean from the returned count. An initial zero count caused error 87, rejected v2 detection, and sent SQ2 into v3 detection. The different CRC (0x54fcc rather than the native v2 0x8e29b) resulted from a different file set; the later attempt to open dir. was a downstream symptom. The launcher permits zero first-search counts to mean one result. Validation and missing-file errors remain active; FindNext stays strict. This does not fix the helper's wildcard shortcut or establish a beta-vs-GA inversion of API return codes.

**Window dimensions.** Sarien asks for frame width 320 and height 200+titlebar-2. Direct native outer sizing reduced the drawable client and clipped the top/right of bottom-aligned output. The launcher enables a compatibility conversion to client width 320 and derived height 200, then AdjustWindowRectEx adds the native nonclient area. Default frame sizing is unchanged. This convention is specific to the existing port, not a general PM geometry specification.

## Validation boundaries

Linux tests use production helpers with mocked WHP registers/time and native window/audio services, compiled with GCC and ASan/UBSan. They validate marshaling, scheduling transitions, bounds, palette/bitmap operations and geometry arithmetic. They cannot establish actual WHP execution, Windows rendering, audio quality or C/386 compilation. Those live results above came from Jason's local builds and runs.

The milestone packaging changes only release text in the C host and documentation/evidence. No additional runtime feature was added after the final successful Windows test.

## Known limits

- One WHP virtual processor; cooperative guest scheduling. Compute-only loops can starve other guest threads and the UI. No general timer preemption; FPU/SIMD context is not saved per guest thread.
- A fixed 64 MiB guest memory model and bounded resource tables, including 32 concurrent thread slots. Completed slots and runtime-owned stacks are recycled; this is not unlimited concurrent threading.
- A narrow PM implementation: one main owner thread, class, message queue and frame/client pair. Worker initialization/posting and shared graphics support exist for the tested path. No general desktop, full widget/dialog/menu-resource implementation, or complete keyboard-layout handling.
- Sarien draws its own game menus into the bitmap. Their appearance does not demonstrate native PM menu APIs.
- Bounded bitmap/DC/PS tables and selected GPI operations. No claim of comprehensive GPI drawing, raster operations, regions or format conversion.
- Beep playback uses native Windows Beep workers with cooperative guest delay. The user hears audio but describes it as muted; timing/device fidelity remains a refinement.
- Queues are process-local; broader process/session behavior and missing APIs require separate work. The earlier os2host32 CMD milestones do not automatically apply here.
- Passing SQ2 does not establish AGI v3 compatibility; the existing application's file-existence helper remains imperfect.

## Suggested next work

Keep this milestone intact and branch subsequent work. Suitable next tasks are improved beep synthesis, additional PM SDK samples, or a CPU-backend abstraction for an optional software x86 core. A software core is a proposed fallback, not implemented here. Choose one explicit next goal before expanding the API surface. Preserve small C/386 regression fixtures and use actual traces to select new behavior.
