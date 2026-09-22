# Start here — Milestone 31C: HANOI

M31B FINAL is frozen and proven.  Milestone 31C should start by scanning and
running the untouched historical HANOI SDK sample against the existing host,
then add only the generic PM behavior that HANOI exposes as missing.

Carry forward these regression gates unchanged:

    make m31a-final-check
    make m31b-final-check

Expected baseline already available to HANOI:

- flat 32-bit LE execution and relocation,
- DOSCALLS and existing PMWIN/PMGPI compatibility personalities,
- native PM window/message bridge,
- M31A 256 KB runtime promotion for tiny PM stacks,
- painting/text/PS operations used by WMCHAR,
- keyboard and WM_CHAR translation,
- recursive LE menu-resource translation,
- OS/2 color-icon translation,
- menu check/attribute updates and native-to-guest command dispatch.

First task: inventory HANOI's imports and embedded LE resources before adding
APIs.  Preserve the original executable as a regression artifact and avoid
sample-specific host code.
