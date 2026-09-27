# ReactOS OS2SS LE4IO handoff

## Baseline preserved

- ReactOS HEAD: `091855fc4f9de8052c8cf4a55830580aab5558da`
- subsystem-5 OS2BOOT architecture preserved
- persistent OS2SS multi-launch path preserved
- streaming visible-console output preserved
- protocol version 1 / `OS2_API_MESSAGE` 0x38 preserved
- LE relocation/materialization/native-stack gateway preserved
- no ReactOS modifications
- no remote-process VM/context APIs
- no VIO/ANSI/KBD implementation

## Added

- DOSCALLS.230 DosGetDateTime through OS2SS
- DOSCALLS.281 DosRead
  - handle 0: interactive visible console through OS2SS + launcher ReadConsoleA
  - local HFILE >=3: native NtReadFile
- narrow temporary native file backend for the exact Infocom imported surface:
  - 223 QueryPathInfo
  - 257 Close
  - 259 Delete
  - 272 SetFileSize
  - 273 Open
  - existing 256 SetFilePtr now handles local files
  - existing 282 Write now handles local files
- dynamic ordinal veneer generation based on the parsed import catalogue
- generalized execution guard for supported OFF32/REL32 LE images
- launcher optional image argument

## Exact uploaded fixture evidence

### phoon.exe

SHA256 `dae30480319c6658fa0af7cd300be68df008d82c151ab77abcafd05c11617f87`

Host construction oracle:
- internal 1045/1045 verified, mismatches 0
- external 18/18 verified, mismatches 0
- entry `0x01003f9c`
- stack top `0x010178a0`
- initial ESP `0x0101788c`

It now has veneers/handlers for all ten imported DOSCALLS, including 230 and 281. x87/387 execution itself remains runtime-untested in this build.

### infocom.exe

SHA256 `da3f8e7f77ddcdeb7a2cee654f61bae4666b7b8802d1fa1f3966d515c424ccb4`

Host construction oracle:
- internal 753/753 verified, mismatches 0
- external 28/28 verified, mismatches 0
- entry `0x0100341c`
- stack top `0x01013070`
- initial ESP `0x0101305c`

Imports covered: 223,224,230,234,256,257,259,272,273,281,282,299,304,305,348.

Interactive execution still requires the program's external story/data file to be present where its own DosOpen path expects it.

## Manual examples

```text
OS2LE4CLAUNCH.EXE phoon.exe
OS2LE4CLAUNCH.EXE infocom.exe
```

If no target is supplied the launcher still defaults to `hi.exe`.

## Runtime truth

`runtime_tested=false` for this package. The builder did not run ReactOS/QEMU/Bochs.
