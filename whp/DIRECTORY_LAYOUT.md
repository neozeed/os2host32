# WHP subtree organisation

The WHP runtime is a self-contained loader subtree.  Its execution-engine
history moves at a different pace from the native OS2HOST32 loader, so WHP
milestone material stays under `whp/`.  Code which is genuinely shared by both
loaders belongs in the repository-level `common/` tree.

## Active product

`src/` contains only source private to building `whp_os2_v2_hi.exe`: the WHP
loader, guest scheduler, guest process/thread state, synthetic import veneers,
guest-memory adapters, callbacks, and subsystem backends.  Do not put guest test
programs or milestone backup copies in `src/`.

The top-level WHP `Makefile` must remain a production build whose default and
only product is `whp_os2_v2_hi.exe`.  It may compile source from `../common/`, but
must not grow test binaries or copied native DLL outputs as incidental targets.

## Shared personality boundary

Ordinary OS/2 semantics which both loaders can implement through a pointer-safe
backend contract belong under `../common/`.  WHP provides adapter operations for:

- validating, reading, mapping, and writing 32-bit guest addresses;
- translating guest HFILE values;
- performing host I/O for a guest request;
- allocating and tracking guest linear memory;
- loader-specific state required by otherwise common semantics.

Execution-engine operations remain under `whp/src/`: virtual-thread scheduling,
blocking waits, process creation, callback transitions, guest exit, and far/16-bit
transitions.  Do not move those into `common/` merely to make source names match.

## Historical material

`docs/current/` is for the current WHP milestone handoff and validation notes.
Superseded milestone packages go to `docs/milestones/`; revision-series notes and
patches go to `docs/milestones/revisions/`.

Do not merge WHP milestone notes into the main project's native-loader milestone
history merely because both loaders implement OS/2 behaviour.

## Tests and evidence

`tests/guest/` is for OS/2 binaries/source whose purpose is to exercise WHP.
`tests/host/` is for WHP-specific host-side mocks and regression checks.  Portable
shared-personality tests belong at repository level under `tests/common/`.

`fixtures/` contains stable guest executables/input material.  `validation/`
contains captured results proving a particular WHP revision worked.  Neither is
product source.

`scripts/` retains build/run helpers for guest programs and historical
reproduction.  They may describe old layouts; retain them rather than rewriting
history unless a new canonical script is explicitly added.

## Source and milestone hygiene

Layout-only work should preserve supplied source contents.  Functional WHP
changes should have their own milestone note, catalogue/wiring validation, and
Windows regression results.  When a new milestone replaces the current handoff,
move the old current files into a named directory below `docs/milestones/` rather
than leaving mixed generations in `docs/current/`.
