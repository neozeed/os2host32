# Milestone 29I - clean C/386 DosFindFirst / DosFindNext ABI

M29H proved the complete seven-thunk VIO/KBD console backend.  M29I keeps that
loader/bridge baseline frozen and cleans the remaining C/386 compiler warnings
on the direct directory-enumeration path.

## Why C386 warned

`cmdos2_os2.c` deliberately owns a private `CmdFileFindBuf3` wire structure so
it does not depend on the exact typedef spelling present in one prerelease SDK
header set.  Its layout is the 32-bit `FIL_STANDARD` / `FILEFINDBUF3` layout:
`oNextEntryOffset`, six packed date/time words, 32-bit file/allocation sizes,
32-bit attributes, name length and name.

The Microsoft header prototype, however, gives the result-buffer parameter its
own SDK pointer type.  Passing `struct CmdFileFindBuf3 *` directly therefore
made C386 diagnose C4049/C4024 even though the buffer ABI was intentional.

M29I makes the ABI boundary explicit by passing `(PVOID)&fb` to `DosFindFirst`
and `DosFindNext`.  No bytes or field offsets change.

## Runtime regression

`cmdos2_os2_find_test.exe` creates:

* `m29i-find-a.tmp` (1 byte)
* `m29i-find-b.tmp` (4 bytes)

It enumerates `m29i-find-?.tmp`, requiring one result from `CmdO2FindFirst` and
the other through `CmdO2FindNext`, validates both names and sizes, closes the
search handle, deletes both fixtures, and prints:

    M29I_DOSFIND_BACKEND_OK

Build with `build-os2-backend.cmd`, then run:

    os2host32 --run cmdos2_os2_find_test.exe

The compile of `cmdos2_os2.c` should no longer emit the old parameter-4 /
parameter-2 C4049/C4024 warnings.
