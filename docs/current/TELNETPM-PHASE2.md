# TELNETPM phase 2: native bridge and execution probe

This package contains a built **32-bit Windows EXE**, loader source, an
incremental patch against phase 1, and tests. It adds an explicit native
execution probe for the exact TELNETPM specimen. It does not establish that
the UI runs, and it does not implement TCP/IP or the remaining PM APIs.

## Run on Windows

Keep your current PMWIN, PMGPI and DOSCALLS DLLs. Put the new loader beside
those compatibility DLLs, or make that directory available through Windows
`PATH`. You can rename the new EXE to `os2host32-phase2.exe` for an isolated
test; leave the DLL filenames unchanged. The input TELNETPM.EXE is not edited.

```bat
os2host32-phase2.exe --telnetpm-check C:\temp\pack\tcpip\x\BIN\telnetpm.exe > phase2-check.txt 2>&1
os2host32-phase2.exe --telnetpm-probe C:\temp\pack\tcpip\x\BIN\telnetpm.exe > phase2-run.txt 2>&1
```

If you retain the packaged filename, use `os2host32.exe` instead. Send back
`phase2-run.txt`, including its beginning and final lines. No environment
variables are required to enable tracing.

- `--telnetpm-check`: reconstruct, relocate and generate the bridge offline;
  no DLL loading and no guest execution. Returns 0 on success.
- `--telnetpm-probe`: load available native compatibility DLLs, install the
  matched bridge, publish PM resources when PMWIN is available, then enter
  the original guest startup code. Every import call is logged.
- An unavailable import stops with exit status **4**, identifying its module,
  ordinal and guest caller. No fabricated return value is supplied.
- An unsupported authentication thunk stops with exit status **5**, including
  mode, target token, packed-frame length and guest caller.
- A changed specimen or loader error fails before guest execution where
  detected; fingerprint failures return 1. A guest fault uses the existing
  register/access-violation diagnostic. A normal entry return is logged and
  its result becomes the process exit code.
- Ordinary `--run` and `--fixups` keep their existing mixed-image guard. Use
  the explicit probe command for phase 2.

The network modules `so32dll` and `tcp32dll` always receive stop-on-call
traps in this mode, even if files with those names are present. Other missing
imports also receive traps. Resolving a trap is not implementing an API.

## What was learned

The six direct calls to the generic dispatcher at object 1:+1CF14 belong to
the optional authentication path. Nearby dynamic procedure lookups identify
`INIT_SEC_CLIENT`, `RELEASE_SEC_CLIENT` and `INIT_AUTH_CONTEXT`. Two calls
invoke a local callback rather than a dynamically loaded procedure.

| Dispatcher call, object 1 | Target / frame |
| --- | --- |
| +BAF7 | `RELEASE_SEC_CLIENT`, 4 packed bytes |
| +BC01 | `INIT_SEC_CLIENT`, 8 packed bytes |
| +BC56, +BD1C | `INIT_AUTH_CONTEXT`, 14 packed bytes |
| +BC89, +BCC9 | Local callback at +B47C, 12 packed bytes |

These roles come from the executable's strings, stores into the procedure
pointer globals, and call-site disassembly. Whether a particular user action
takes each path still requires a live run. External authentication DLLs and
their calling conventions are not implemented by this milestone.

The local callback accepts four packed arguments, in ascending stack order:
32-bit pointer, 16-bit length, 32-bit pointer, 16-bit length. Its original
16-bit prologue widens them and enters the 32-bit body at +B4D4. The replacement
dispatcher accepts only the observed mode=1, local target and 12-byte frame;
it widens the arguments, calls the original body, zero-extends its 16-bit
result, and leaves argument cleanup to the caller.

The body constructs TELNET subnegotiation output and doubles `0xFF` bytes in
its second input buffer. Tests execute that original machine code. The send
routine is intercepted only in the tests, so those checks perform no network
I/O and do not assert that sockets work.

## Implementation

- Full-file SHA-256 recognition, including code, relocation tables, imports
  and resources. Only this specimen is admitted:
  `16f34d712cdefb8b3f4fa93b7956fc97429697419deccd80b62b8d838d4960e2`
  (273,544 bytes). Renaming it is fine; editing it requires a new analysis.
- All 7,779 internal and 567 external relocation sites are applied to real
  native mappings in probe mode. The internal REL32 into `MSGSEG32` is handled.
- The two pushed alias callback pointers become flat callback tokens. The
  other five non-flat sites are in replaced, unreachable marshalling code.
- The generic dispatcher is replaced by a checked native dispatcher. The
  local callback's segment-switch prologue and far-return epilogue are
  bypassed; the original 32-bit body is retained. Superseded transition bytes
  are filled with breakpoint instructions. Object 2 is made inaccessible in
  the native process, so the original 16-bit shim cannot execute.
- The two EAX selector-conversion entrypoints at +246F8/+24700 become flat
  token identities. The original unsafe stack/segment dispatcher is never
  used to consume those tokens.
- **101 direct `FS:[0]` operations** are replaced with generated native
  trampolines: 21 pushes, 20 stores of ESP, 59 pops and one store of EAX.
  These operate on a per-thread OS/2 exception-chain shadow. Win32's real FS
  and exception chain remain untouched.
- Loader-local DOSCALLS.354/.355 adapters use the same shadow chain.
  DOSCALLS.312 wraps the existing backend, returning a per-thread copy of its
  TIB with a consistent exception-chain field. This does not add OS/2
  exception dispatch, TLS allocation APIs, or new worker-thread semantics;
  the backend's existing TID/stack reporting limitations remain.
- Import tracing and inline FS trampolines preserve general registers,
  flags and x87 state across their host observer calls. Import wrappers tail
  jump with the original arguments, return address and AL intact. x87 state
  is saved/restored using x86 FNSAVE/FRSTOR, without requiring SSE/FXSAVE.
- Native stubs become execute/read after construction. No CPU selector
  descriptors are installed. No guest DLL is initialized while resolving
  this probe's static imports; existing dynamic module APIs remain as before.

All new runtime behavior is behind `--telnetpm-probe`. The existing flat,
C/386 and EMX paths are unchanged. PMWIN, PMGPI, DOSCALLS source and DLLs are
not included in this overlay; keep your newer SimCity work.

## Source integration

The complete `loader/` overlay includes phase 1 and phase 2. If your loader
still matches our phase-1 version, copy the files into the project and build:

```sh
make -B os2host32.exe
```

If you have edited the loader since phase 1, use the incremental patch and
review any conflicts instead of overwriting your edits:

```sh
git apply --check loader-phase2.patch
git apply loader-phase2.patch
git apply --check makefile-phase2.patch
git apply makefile-phase2.patch
```

The first patch adds the bridge header and tests and updates the main loader.
It assumes `loader/mixed_intake.h` from phase 1 is already installed. The
optional makefile patch adds the new header dependencies and portable test
target. The existing native build recipe works without it when forced with
`-B`. No full old makefile is supplied to overwrite unrelated changes.

## Validation

The delivered EXE cross-builds with i686 MinGW GCC 13.2.0 and is PE32/i386.
No Windows runtime is available in the build worker, so **this is a built and
emulator-checked probe, not a live Windows TELNETPM success claim**.

```sh
make telnetpm-bridge-check TELNETPM=/path/to/telnetpm.exe
make mixed-intake-check TELNETPM=/path/to/telnetpm.exe
```

The bridge tests require Python 3 and `unicorn`; the production EXE has no
Python/emulator dependency. `tests/loader/telnetpm_emit.c` invokes the exact
production emitters to create a temporary test snapshot; it is not a second
implementation of the bridge and embeds no historical executable.

- **271 checks pass**, including SHA-256 padding vectors, changed-specimen
  rejection, unchanged ordinary run/fixup guards, all 137 import wrappers,
  all 101 inline FS replacements, the two flat-token helpers, callback data
  marshalling/escaping, unsupported thunk rejection, internal REL32 and
  callback-pointer relocation, and the retired object-2 bytes.
- The original entry runs under Unicorn through its first inline exception
  chain push to **DOSCALLS.331**, returning to object 1:+1DA67. No API result
  is simulated for that test; it stops at this first import boundary.
- The same 271 tests pass with production emitters built under ASan/UBSan.
  Leak detection is disabled because the existing loader exits immediately
  on rejected inputs. Windows TLS, LoadLibrary, PM resources and real API
  behavior await the user's live run.
- Phase-1's 30 synthetic cases and the independent seven-object relocation
  oracle pass. API catalogue, C/386 bridge and SimCity timer static checks
  pass. The Windows compiler emits existing loader warnings; none originate
  in `telnetpm_bridge.h`.

Build and test logs are in `docs/current/telnetpm-phase2/`. The next result we
need is the live probe's first unavailable API or guest fault. A PM window
is a later milestone once those concrete blockers have been implemented.
