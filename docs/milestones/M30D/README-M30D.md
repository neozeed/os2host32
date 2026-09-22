# Milestone 30D - expose the mapped LX stack through DosGetInfoBlocks

M30C successfully rebased the retained EMX a.out absolute relocations.  The
minimal bound `hi.exe` advanced past the previous:

    Invalid option in .exe file

and reached the next EMX startup check:

    emx.dll: Stack size too small.  Run
      emxstack -f hi.exe
    and try again.

That message is a false positive in our host.  The executable already has an
8 MB LX stack object and runs unchanged on real OS/2.

## Root cause

EMX calls DOSCALLS.312 (`DosGetInfoBlocks`) during startup.  The OS/2 TIB
layout has:

    +0x00  tib_pexchain
    +0x04  tib_pstack
    +0x08  tib_pstacklimit
    +0x0c  tib_ptib2

The supplied EMX DLL reads +4 and +8, subtracts them, and rejects a stack span
of 0x4000 bytes or less.

Before M30D our compatibility implementation did:

    memset(&o2_info_tib, 0, sizeof(o2_info_tib));

and never populated either stack field.  EMX therefore saw a zero-byte stack
and emitted the `emxstack` diagnostic even though os2host32 had mapped a real
large stack object.

For the current `hi.exe` mapping M30C showed:

    stack object actual base : 03020000
    initial ESP / stack top  : 03820000
    usable span              : 00800000 (8192 KB)

## M30D change

`os2host32.exe` now publishes the mapped main guest stack through a private
in-process export:

    OS2HostQueryMainStack

The bounds are established immediately after mapping the main executable,
before imported guest DLLs are initialized.

`DOSCALLS.dll` uses that private query in `DosGetInfoBlocks` and returns:

    tib_pstack      = mapped stack object base
    tib_pstacklimit = initial guest stack top

M30D also supplies a minimal TIB2 with TID 1 and points `tib_ptib2` at it.
This matches the main-thread model already used by the M30 EMX FS/TIB bridge.

No change is made to `hi.exe`; **do not run emxstack for this test**.

## Build

Use the same i686 MinGW setup as M30C:

    make clean
    make tools compat

Copy into the build directory:

    hi.exe     - exact bound executable that runs on real OS/2
    emx.dll    - EMX runtime DLL
    hi         - the unbound ZMAGIC sidecar (included in this package)

Then run:

    m30d-emx-hi-test.cmd

Expected diagnostics include something like:

    M30D TIB stack   : 03020000..03820000 (8192 KB)
    ...
    M30D TIB: pstack=03020000 limit=03820000 span=8192 KB tid=1

The old stack-size diagnostic should disappear.  The ideal next output is
simply:

    hi

If execution advances to another EMX diagnostic, deferred import, or guest
exception, capture the complete output from the `M30C EMX` relocation lines
through the failure.

## Runtime-version note

The EMX DLL used during the current investigation identifies itself internally
as EMX 0.9c rev 50, while the user's rebuilt `emxbind` reports 0.9d.  That is
worth normalizing later, but the M30C stack failure itself is explained
completely by our zero-filled TIB stack fields.
