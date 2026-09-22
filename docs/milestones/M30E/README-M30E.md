# Milestone 30E - EMX / OS/2 exception-registration chain

M30D advances the real EMX-bound `hi.exe` through:

* a.out absolute relocation rebasing,
* EMX executable-option parsing,
* the OS/2 TIB stack-size sanity check,

and then stops at the first actually-used deferred import:

    DOSCALLS.354

OS/2 ordinal 354 is `DosSetExceptionHandler`.

## M30E behavior

M30E implements the OS/2 `EXCEPTIONREGISTRATIONRECORD` linkage semantics used
by normal runtime startup.  The record is two dwords:

    +0  prev_structure
    +4  ExceptionHandler

`DosSetExceptionHandler` now:

1. stores the current chain head in `prev_structure`,
2. makes the supplied record the new main-thread chain head,
3. mirrors that head in the compatibility TIB's `tib_pexchain`,
4. returns `NO_ERROR`.

The current M30 EMX execution model is intentionally single-threaded, so one
process-local guest exception chain is sufficient for this milestone.

M30E does **not** yet translate Win32 faults/SEH exceptions into OS/2 exception
records or invoke the guest handlers.  This is registration semantics only,
which is what the normal `printf("hi\\n")` startup path has reached so far.

Ordinal 355 (`DosUnsetExceptionHandler`) is included as the matching chain
teardown operation even though this exact EMX DLL does not statically import
it.  No broader exception API (such as DosUnwindException) is implemented by
this milestone.

With `OS2_TRACE_EMX_SELF=1`, registrations look like:

    M30E EXCEPT: DosSetExceptionHandler rec=........ prev=FFFFFFFF handler=........

Success means the old deferred-import diagnostic for DOSCALLS.354 disappears.
The next line may be another deferred API; that is useful and should be treated
as the next narrow compatibility target.

## Build and test

Use the same i686 MinGW environment as M30D:

    make clean
    make tools compat

Place beside the resulting binaries:

    hi.exe     known-good EMX-bound executable
    emx.dll    EMX runtime DLL
    hi         unbound ZMAGIC a.out sidecar (already included)

Then run:

    m30e-emx-hi-test.cmd
