# Read first — WHP Milestone 3 shared personality

Milestone 3 removes the first duplicated DOSCALLS implementations from the WHP
loader and makes native `DOSCALLS.DLL` and WHP compile the same common source.

## Build

From an x64 Visual Studio Developer Command Prompt:

    cd whp
    make clean
    make

Only `whp_os2_v2_hi.exe` is produced.

From the repository root, `make whp` invokes the same build.

## What changed

- A canonical API ordinal catalogue now names and classifies all compatibility
  DLL exports plus the WHP-only DOSCALLS ordinals.
- Eight DOSCALLS implementations are shared by native and WHP.
- WHP uses a guest-safe adapter; no common routine dereferences a guest address
  as a Win64 pointer.
- Date/time acquisition and monotonic time are common Win32 services.
- The transformer and WHP diagnostics resolve ordinal imports to API names.
- Threading, processes, semaphores, callbacks, sleep, and other virtual-CPU
  operations remain WHP intrinsics.

Read `MILESTONE3-SHARED-PERSONALITY.md` for the detailed boundary and
`NEXT-CHAT-HANDOFF.md` before continuing development.

For review and archival:

- `M2-TO-M3-SHARED-PERSONALITY.patch` is the complete source/documentation
  transition from the integrated Milestone 2 tree to this milestone.  It does
  not include itself or the checksum manifest.
- `SHA256SUMS-MILESTONE3.txt` covers every packaged file except itself and the
  repository's private `.git/` directory.

The previous current handoff is preserved unchanged under:

    whp/docs/milestones/m2-package/
