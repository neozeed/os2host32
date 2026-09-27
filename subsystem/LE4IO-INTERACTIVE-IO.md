# LE4IO interactive I/O architecture

## Console input

```text
LE DosRead(0,pBuf,cb,&actual)
  -> ordinal 281 veneer
  -> OS2BOOT native-stack gateway
  -> Os2ApiDosRead(handle=0, shared offset/length)
  -> OS2SS
  -> LE4C temporary console bridge
  -> CSR-registered launcher main thread
  -> ReadConsoleA(StandardInput)
  -> acknowledged bytes back to OS2SS
  -> R5 shared view
  -> OS2BOOT copies bytes to LE pBuf
```

One console bridge LPC action carries at most 192 input bytes because ReactOS x86 LPC messages are capped at 256 bytes. This is valid partial-read behavior; the application may call `DosRead` again. Console line discipline/echo is provided by ReactOS ConSrv, not by VIO/KBD emulation.

## Console output

The already-live-proven streaming path is unchanged. A semantic OS/2 DosWrite may be up to the 32 KiB shared-view limit and is streamed to the launcher in acknowledged 192-byte LPC chunks before OS2SS returns success/actual.

## Date/time

`DosGetDateTime` uses native system time converted to local `TIME_FIELDS`, then returns the historical 12-byte OS/2 DATETIME shape. Timezone is presently zero in this proof backend; local calendar/time fields are correct.

## File handles

HFILE 0/1/2 remain standard input/output/error.

HFILE 3..31 are a bounded OS2BOOT-local table of native NTDLL file handles. This intentionally avoids cross-process buffer pointers and gives LE code directly usable local file I/O without inventing an OS2SS-wide filesystem subsystem yet.

Implemented native operations:

- `NtCreateFile` for DosOpen
- `NtReadFile` / `NtWriteFile`
- `NtQueryInformationFile` / `NtSetInformationFile` for seek/size
- `NtQueryFullAttributesFile` for path info
- `NtDeleteFile`
- `RtlDosPathNameToNtPathName_U` / `RtlGetFullPathName_U`

This is a temporary compatibility backend, not final filesystem translation semantics.
