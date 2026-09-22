# M28D5 - isolate process/pipes from recovered C/386 stdio

M28D4 proved that the failing `upper-test.exe` is not a CMD parser/pipeline-only
problem: it exits 2 even when run directly as

    os2host32 --run upper-test.exe < input > output

Its only new DOSCALLS import versus the write-only producer is `DosRead` (281).
M28D5 therefore separates two questions that had become entangled:

1. Do inherited OS/2 HFILE 0/1 and the asynchronous pipeline work?
2. Why does the recovered C/386 `getchar`/`putchar` combination fail?

`upper-dos-test.exe` is a genuine C/386 LX filter, but uses `DosRead(0)` and
`DosWrite(1)` directly.  The M28D5 pipeline regression uses this program so the
process/pipe milestone can be validated independently of C stdio buffering.

`DOSCALLS.dll` also has optional tracing controlled by `OS2_TRACE_IO=1` for:

- DosQueryHType (224)
- DosSetFilePtr (256)
- DosRead (281)
- DosWrite (282)

The trace is written to stderr and includes the OS/2 HFILE, native HANDLE type,
requested byte count, result/error, and actual byte count.

A separate `examples\\m28d5-stdio-probe.cmd` is intended to be run from normal
Windows CMD.  It compares the direct OS/2 API filter with the stdio filter.

M28D5 also fixes an accidental duplicate `EnterCriticalSection(&child_lock)` in
the asynchronous-child table.  Windows critical sections are recursive, so the
old code did not immediately deadlock the same thread, but it left an unmatched
recursion count and was incorrect for future concurrent waits.
