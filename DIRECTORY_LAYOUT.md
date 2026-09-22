# OS2HOST32 Directory Layout and House Rules

This file defines where material belongs in the repository.  The goal is to keep
active product source obvious, keep historical material available without mixing
it into the build, and prevent the project root from becoming a milestone archive.

## Active build tree

### `common/`
Loader-neutral OS/2 API semantics and metadata shared by the native compatibility
DLLs and the WHP loader.  Keep explicit 32-bit application addresses at this
boundary; do not pass a WHP guest pointer as a Win64 host pointer.

- `common/include/` contains stable shared contracts.
- `common/api/` contains the canonical module/ordinal/name catalogue.
- `common/doscalls/` contains shared DOSCALLS semantics.
- `common/win32/` contains Win32 services which are genuinely identical for both
  execution backends.

An API belongs here only after native and WHP both call the same common
implementation.  Loader scheduling, process control, callbacks, and other
execution-engine operations do not belong here merely to avoid two function names.

### `loader/`
The OS2HOST32 loader/runtime executable and code directly belonging to it.

Do not place compatibility DLL implementations here.  Loader-specific helpers may
live here when they are genuinely private to the loader.

### `whp/`
The alternative Win64 Windows Hypervisor Platform loader/runtime.  WHP is an active
peer loader, but its source, milestone notes, guest experiments, validation captures,
and historical scripts are intentionally self-contained below `whp/`.

The root project may provide an explicit `make whp` convenience target, but WHP is
not part of the normal MinGW `all` target because it uses the Win64 Microsoft
toolchain and `WinHvPlatform.lib`.  See `whp/DIRECTORY_LAYOUT.md` for its internal
organisation rules.

### `transformer/`
LE-to-PE transformation/conversion tools, currently including `le2pe386`.

### `shell/`
User-facing OS/2 personality programs that are part of the normal build, currently
CMD32.

### `dlls/`
Compatibility/personality DLL exports, native adapter code, and subsystem-private
source.  Shared semantics should live under `common/`; the DLL files should become
thin ABI veneers as APIs are migrated.  Keep each OS/2 API family in its own
subdirectory, for example:

- `dlls/doscalls/`
- `dlls/kbdcalls/`
- `dlls/viocalls/`
- `dlls/quecalls/`
- `dlls/sesmgr/`
- `dlls/nls/`
- `dlls/pmwin/`
- `dlls/pmgpi/`
- `dlls/pmshapi/`
- `dlls/pmwp/`
- `dlls/helpmgr/`

Shared PM-only declarations belong in `dlls/pm-common/`.  Do not create a generic
catch-all DLL directory for unrelated code.

## Tests, examples, and evidence

### `tests/`
Regression tests and compatibility probes.  Portable tests for the common
personality layer belong under `tests/common/`.  Historical milestone-specific
tests belong under `tests/milestones/`; older retained tests that are no longer
part of the normal regression path belong under `tests/legacy/`.

### `examples/`
Example applications intended to demonstrate supported OS/2 behaviour.  Examples
are not implementation source and should not be linked into the runtime.

### `fixtures/`
Input files and other stable test fixtures needed to reproduce tests.

### `regression-evidence/`
Captured output or other evidence demonstrating a known-good result.  This is not
build input.

### `analysis/`
Reverse-engineering output, scans, API inventories, and other generated or working
analysis material that is useful to retain but is not product source.

## Documentation

### `docs/current/`
Documentation for the current working milestone: handoff notes, the original
milestone Makefile, current snapshot notes, and similar material.  Keep this small.

### `docs/milestones/`
Completed milestone notes and historical handoffs.  When a milestone is superseded,
move its working notes here rather than leaving copies in the repository root.

### `docs/notes/`
Implementation notes, change descriptions, compatibility observations, and other
project-authored reference material that is not itself a milestone package.

### `docs/reference/`
External historical/reference documentation.  Reference material should be kept
separate from project-authored notes and, where practical, preserved verbatim.

The Microsoft Programmer's Library OS/2 material lives under:
`docs/reference/microsoft-programmers-library/`.

Do not silently edit historical/reference texts.  Put project annotations in
`docs/notes/` instead.

## Scripts and tools

### `scripts/`
Project automation.  Obsolete or milestone-specific command files/scripts that
are retained only for reproducibility belong under `scripts/legacy/`.

### `tools/`
Standalone development/inspection utilities that are useful to the project but
are not normal runtime deliverables.

## Archive

### `archive/`
Retained obsolete artifacts, old binaries, source backups, patches, and similar
material that should not participate in the current build.  Nothing under
`archive/` should be treated as the canonical implementation.

## Root directory policy

The repository root should normally contain only:

- build entry points such as `Makefile`;
- top-level project documentation such as `README.md` and this file;
- the major directories described above;
- build products produced at the root when compatibility/runtime discovery
  requires the EXE and personality DLLs to sit together.

Do **not** add milestone notes, experimental C files, patch fragments, backup source
files, ad-hoc test executables, or captured logs to the root.

## Source preservation rule

When reorganising the repository, moving a source file is fine; changing its
contents is a separate operation.  Repository-cleanup milestones should preserve
`.c`, `.h`, and `.def` contents unless source changes are explicitly part of the
requested work.  Functional migrations such as the shared personality core must
be documented and validated independently from layout cleanup.

## Adding a new subsystem

For a new OS/2 API family, create `dlls/<subsystem>/` and keep its export veneer,
DEF file, native adapter, and subsystem-private headers together.  Put semantics
shared with WHP in `common/<subsystem>/`.  Add the resulting DLL to the root build
only when it is intended to be a normal runtime deliverable.

For an experimental API implementation that is not yet part of the product, keep
it under the relevant test/analysis area until it is promoted deliberately.

## Milestone hygiene

At the end of a milestone:

1. Keep only the current handoff/snapshot material in `docs/current/`.
2. Move superseded milestone notes to `docs/milestones/`.
3. Move obsolete test/build scripts to their `legacy/` locations rather than
   deleting reproducibility material.
4. Keep the active product directories free of backup copies such as `foo-old.c`,
   `foo.bak`, or patch scratch files.
5. Verify that repository cleanup did not alter active source unintentionally.

This layout is deliberately boring: a new contributor should be able to tell what
builds the runtime, what is a test, and what is historical reference material
without first understanding the project's milestone history.
