# Soft386 NE-H3G — historical DOS16 startup and stdin bridge

Baseline: complete frozen NE-H3F source. All changes confined to `soft386/`.

## Implemented
- DOS16.41 `DosGetHugeShift`: writes shift 3 (8-byte GDT selector stride) to guest far-pointer output.
- DOS16.14 `DosSetSigHandler`: guest-process-local handler and action records, returns previous state; registrations only, no asynchronous signal delivery.
- DOS16.72 `DosQCurDisk`: emits guest 16-bit drive and 32-bit logical-drive bitmap. Windows queries `_getdrive` and `GetLogicalDrives`; POSIX smoke-test maps the process to C: only (not a true POSIX drive model).
- DOS16.137 `DosRead`: stdin handle 0 via host read, with guest segmented buffer translation and 16-bit actual length. Other handles return error 6 until a per-NE file handle table exists. Guest buffers/selector memory remain in Soft386.
- Correct Pascal far-call argument cleanup for the above.
- No change to existing support DLLs or 32-bit execution flow.

## Tests
`make check-quick` PASS on POSIX host: six NE executable fixtures (VOID, TINY, MEDIUM, LARGE, COMPACT, HUGE), LIB no-input (EOF) startup, 32-bit LE/thread/sync/memory/bridge checks. `LIB.EXE` no longer emits unsupported 41/14/72/137 messages; banner and filename prompt work and EOF ends with expected OS/2 rc=2. An entered `NOFILE.LIB` causes the next unsupported `DOS16.70` and subsequently `Create? (y/n)`; creating/reading libraries NOT yet supported.

## Caveats and next step
This is a narrow bridge milestone, not full OS/2 1.x filesystem or signal support. Next audit DOS16.70 and implement guest-local file handles + exact historical open/close/seek/read semantics. Validate native Win32 MinGW build and live Windows console behavior before declaring runtime-proven.
