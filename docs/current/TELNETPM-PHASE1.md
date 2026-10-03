# TELNETPM phase 1: mixed-LX intake (no guest execution)

## Run

The supplied PE32 `os2host32.exe` includes a new diagnostic command:

```bat
os2host32.exe --mixed-intake telnetpm.exe > telnetpm-phase1.txt 2>&1
```

No compatibility DLLs, TCP/IP stack, fonts or PM resources on disk are needed.
The input executable is not modified. The command never loads an imported DLL,
calls a guest entrypoint, installs a selector, or allocates executable memory.
Success means the offline intake completed, **not that TELNETPM runs**.

The native `--run` and `--fixups` eligibility gates remain unchanged: this
specimen is rejected with exit status 3. `--mixed-intake` returns 0 on completed
intake, 1 on unsupported/malformed input. Phase 1 supports LX only; use `--scan`
for LE files such as SimCity.

## Source installation / preserving current PM work

This is a loader-only overlay against the supplied `ebb2009` project snapshot.
It contains no PMWIN/PMGPI files, no TCP/IP DLLs and no historical application
binary. Keep your newer SimCity PM DLLs and source unchanged.

For a quick isolated test, use the included executable from a separate folder.
For source integration, copy `loader/os2host32.c`, `loader/mixed_intake.h` and
the tests into the corresponding project directories. The included `loader/`
definition file is unchanged. If your loader has changed since ebb2009, use
`loader-phase1.patch` with `git apply --check` and review the merge instead of
overwriting it. The patch includes the new header and tests.

Build with your existing production makefile:

```bat
make -B os2host32.exe
```

Add `loader/mixed_intake.h` to the prerequisites of `os2host32.exe` for normal
incremental builds. The optional `makefile-phase1.patch` adds that dependency
and a portable test target without replacing your makefile.

Portable test build (Linux/macOS; Python 3 needed only for tests):

```sh
cc -std=c89 -O2 -o os2host32-intake loader/os2host32.c
python3 tests/loader/test_mixed_intake.py ./os2host32-intake /path/to/telnetpm.exe
```

Omit the last argument to run only synthetic fixtures. With the optional
makefile patch: `make mixed-intake-check TELNETPM=/path/to/telnetpm.exe`.

## What is implemented

- Checked normal, zero-fill and iterated LX page reconstruction into separate
  non-executable heap buffers, including BSS and zero/duplicate preferred bases.
- Deterministic, non-overlapping model addresses at 64 KiB boundaries. These
  are numeric model addresses, not locations reserved in the host process.
- Explicit default/16-bit-alias selector tokens. Default32 selectors model a
  base of zero; 16-bit/default and alias selectors model the object's base.
  Tokens are never installed in the CPU or passed into guest execution.
- Internal OFF32, REL32, SEL16 and PTR16:32 relocation in those buffers.
  Source-list records are expanded; biased OFF32 values retain modular
  32-bit arithmetic. Alias selectors are represented explicitly.
- All external REL32 operands remain byte-for-byte unchanged, with deferred
  site counts. There are no fake resolved addresses or success stubs.
- Exact fixup-site/context diagnostics for non-flat and cross-object cases,
  tiny-object raw bytes, three recognized far-jump instruction shapes, and
  per-object FNV-1a fingerprints for independent verification.
- Unknown internal kinds, non-flat external imports, additive/chained records,
  invalid/compressed/range pages and invalid ranges fail closed. The diagnostic
  has a 128 MiB combined object-size cap and accepts 4096-byte logical pages.

The existing fixup scanner now takes an optional visitor. Native scan/run paths
still use it without the visitor. No native `apply_fixups`, import resolver,
C/386/EMX bridge or guest DLL lifecycle code was changed.

## Specimen evidence and correction to the initial assessment

Input: 273,544 bytes, SHA-256:
`16f34d712cdefb8b3f4fa93b7956fc97429697419deccd80b62b8d838d4960e2`.

The earlier statement that object 2 looked like a pointer table was incorrect:
it used a linear physical-page calculation instead of the LX page map. This
implementation follows the actual map. Object 2 comes from file offset
`0x39E00`; object 3 from `0x3A000`.

| Object | Actual contents / role |
| --- | --- |
| 1 | Main 32-bit code **plus a 16-bit alias entry at +B47C** |
| 2 | 35 bytes of genuine far-jump/thunk code |
| 3 | 48-byte `MSGSEG32` data/code wrapper; external `MSG.6` at +27 |
| 4 | Eight zero-initialized bytes, 16-bit default data object |
| 5 | Main data/BSS/stack |
| 6 and 7 | Distinct resource objects despite both preferred bases being zero |

The instruction bytes and selector/far-pointer fixups establish these static
edges (offsets are object-relative):

| Source | Destination | Evidence |
| --- | --- | --- |
| 1:+1CF6E | 2:+000A, 16-bit alias | `66 EA`, selector fixup at 1:+1CF72 |
| 2:+0000 | 1:+B47C, 16-bit alias | `66 EA`, selector fixup at 2:+0006 |
| 2:+001B | 1:+1CF78, 32-bit default | `66 EA`, PTR16:32 fixup at 2:+001D |

At 2:+0019, `FF 1C` decodes as an indirect far call through `[SI]` in 16-bit
mode. Its callee is determined at runtime; phase 1 does not invent a target.
At 1:+1CF69 the surrounding 32-bit helper switches stacks with `LSS SP,[ESP]`;
after the far return it restores the stack and segment registers, including FS.
Those operations cannot simply be allowed to execute against Win32 state.

Two alias-selector fixups at 1:+BC85 and 1:+BCC5 are in pushed far-pointer
arguments targeting 1:+B47C. Decoding +B47C in **16-bit mode** gives an argument
marshalling entry, followed by a constructed far return into 32-bit code at
+B4B0. It is not valid to assume all bytes in object 1 execute in 32-bit mode.
The separate internal REL32 site at 1:+1D4EE calls object 3:+12 and must be
relocated when the object layout changes.

These are static instruction/relocation observations, not a runtime trace or
a whole-program reachability proof. Whether startup or a particular UI action
takes each branch remains unproven. A native bridge must handle the call ABI,
alias entry, stack conversion and segment preservation before enabling --run.

## Validation

On this exact specimen:

```text
INTAKE PASS internal-sites=7779 external-deferred=567 nonflat-sites=7 alias-sites=4 far-jump-edges=3
```

- All seven relocated object fingerprints match an independent Python parser
  and relocation oracle, including unchanged external operands.
- Synthetic fixture suite covers relocation widths, source lists, biased
  offsets, zero/BSS/iterated pages, malformed bounds and the execution gate.
- The same tests pass under AddressSanitizer/UndefinedBehaviorSanitizer with
  leak detection disabled (the existing loader exits on malformed input).
- Existing TELNETPM and SimCity `--scan` output is byte-identical to the original
  loader from the uploaded archive. Existing C/386 bridge, API catalogue and
  SimCity timer static checks pass.
- The Windows PE32 loader cross-build succeeds with i686 MinGW GCC 13.2.0.
  Existing warnings were reproduced with the original unmodified source;
  no Windows runtime test was performed here.
- `make verify` passes the initial common/NLS checks, then is blocked by the
  uploaded archive's missing `whp/src/whp_os2_v2_hi.c`. That absent directory
  predates this work; it was not restored, changed or removed by phase 1.

The next execution milestone is a validated native replacement for this thunk
family, not simply bypassing the mixed-mode guard. PM API/network work remains
out of scope for this package.
