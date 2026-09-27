# ReactOS OS2SS LE4IO — interactive console input + narrow native file backend

Baseline: live-proven LE4C streaming-console / persistent OS2SS architecture on ReactOS HEAD `091855fc4f9de8052c8cf4a55830580aab5558da`.

LE4IO adds the minimum I/O surface needed to move beyond write-only `hi.exe` demos while preserving the subsystem-5 OS2BOOT vessel and external OS2SS personality server.

## New DOSCALLS support

New personality services:

- 230 `DosGetDateTime`
- 281 `DosRead`

For standard input, `DosRead(0,...)` travels through the existing 32 KiB shared view to OS2SS, which asks the console-attached subsystem-3 launcher to perform a blocking `ReadConsoleA` on its CSR-registered main thread. Returned bytes are acknowledged back to OS2SS, copied into the shared view, then copied locally into the untouched LE caller buffer by OS2BOOT.

`DosGetDateTime` is serviced by OS2SS using `NtQuerySystemTime`, `RtlSystemTimeToLocalTime` and `RtlTimeToTimeFields`; the OS/2 12-byte DATETIME layout is copied locally by the gateway. The current experimental backend reports timezone field zero; local date/time fields are native-local.

## Temporary native file backend

The uploaded Infocom fixture imports more than only 230/281, so LE4IO also adds the narrow file surface already proven by the WHP personality:

- 223 `DosQueryPathInfo` — levels 1 and 5
- 257 `DosClose`
- 259 `DosDelete`
- 272 `DosSetFileSize`
- 273 `DosOpen`
- existing 256 `DosSetFilePtr` now works for local file handles
- 281 `DosRead` works for local file handles as well as stdin
- 282 `DosWrite` works for local file handles as well as console stdout/stderr

This is explicitly a **TEMPORARY NATIVE FILE BACKEND**, not general OS/2 filesystem/path semantics. It uses a bounded 32-slot OS2BOOT-local HFILE table backed by NTDLL native file handles. Paths are currently narrowed to the ASCII/DOS-path cases used by these fixtures; EAs and broader filesystem semantics remain out of scope.

## Generalized LE execution guard

The exact-SHA and hi-family fixed-count assumptions are gone. The execution guard now derives counts and imports from the common LE plan and accepts only the currently proven shape:

- LE, two 32-bit objects at preferred `0x10000` / `0x20000`
- internal `OFF32` fixups only
- external ordinal `REL32` imports only
- DOSCALLS imports limited to the explicitly supported set
- no named imports / unsupported records
- every internal/external site still independently applied and verified before `EXECUTION ARMED`

The supported ordinal set is:

`223,224,230,234,256,257,259,272,273,281,282,299,304,305,348`.

## Launcher QoL

The launcher now accepts an optional target image as its first argument:

```text
OS2LE4CLAUNCH.EXE             # defaults to hi.exe
OS2LE4CLAUNCH.EXE phoon.exe
OS2LE4CLAUNCH.EXE infocom.exe
```

The persistent-OS2SS multi-launch behavior remains: if `\OS2SS` already exists the deferred subsystem load is skipped and a fresh subsystem-5 OS2BOOT is submitted directly.

## Host-side fixture validation

All four packaged execution fixtures pass the host construction/fixup oracle without executing LE code:

- `hi.exe`
- `hi2.exe`
- `phoon.exe`
- `infocom.exe`

`phoon.exe`: 1045 internal / 18 external sites, all verified with zero mismatches.

`infocom.exe`: 753 internal / 28 external sites, all verified with zero mismatches.

## Build

```text
make REACTOS=/path/to/reactos
make verify REACTOS=/path/to/reactos
make check-image IMAGE=/path/to/program.exe
make inspect-image IMAGE=/path/to/program.exe
make clean
```

`make verify` performs deterministic target rebuilds, static checks, and host construction oracles for all four packaged fixtures.

## Runtime status

BUILD ONLY in this package. ReactOS/QEMU/Bochs were not run by the builder.

The user should test `phoon.exe` first for the x87 path, then `infocom.exe` with its required story/data file available at the path the program opens.
