# Soft386 I386-H2Q — PMSHAPI.117 boundary diagnostics

Date: 2026-10-07
Parent: I386-H2P guest-memory watchpoint milestone
Status: source-side diagnostic milestone; Windows runtime pending

## Why H2Q exists

H2P disproved the working hypothesis that Tiny386, callback re-entry, or a
host copy-back was repeatedly corrupting NEKO's timer state.  Watching guest
0x00020068:0x20 for the full live run produced 1070 change records, and every
actual changed byte was 0x0002007C.  No write to the state selector at
0x00020074 was observed.

The H2O/H2P instruction trace shows the WM_TIMER callback repeatedly taking the
same state-2 path:

    0001083A  movzx eax,word [00020074]
    ...
    00010845  je    00010855
    00010855  mov   word [0002007C],0
    ...
    00010987  inc   word [0002007C]
    0001098E  mov   ax,word [000211E0]
    00010994  cmp   word [0002007C],ax
    0001099B  jb    ...

In the Soft386 run the word read from 0x000211E0 is zero, so counter 1 cannot
satisfy the unsigned-below test.  The movement path is therefore never entered.
The same run also shows an earlier generic initialization discrepancy:

    PMSHAPI.117 PrfQueryProfileData -> FALSE
    WinStartTimer(... timeout=0)

The proven native os2host32 NEKO trace opens C:\\OS2\\neko.ini and starts the
same cat timer at 0xC8 (200 ms).  H2Q therefore moves the investigation upstream
to the shared PMSHAPI profile-data boundary instead of changing Tiny386 or PM
rendering.

## H2Q changes

H2Q changes no OS/2 API behavior.

When --trace-native is enabled, the Soft386 PMSHAPI.117 marshaller now prints:

- guest HINI token and translated native HINI;
- bounded app/key strings already copied from guest memory;
- guest data-buffer and pcb addresses;
- incoming pcb value;
- provider BOOL result;
- outgoing pcb value;
- up to the first 16 decoded bytes from the native marshalling buffer.

The shared PMSHAPI provider gains OS2_PM_TRACE diagnostics for
PrfQueryProfileData.  It prints:

- resolved profile path;
- HINI, app and key;
- incoming pcb and whether a data buffer was supplied;
- GetPrivateProfileStringA character count and the first 32 characters;
- a precise failure class: unknown HINI, allocation, missing/non-@HEX value,
  short buffer, invalid hex;
- or success byte count and up to the first 16 bytes.

The provider continues to use the existing @HEX: binary-profile representation.
No NEKO name, resource ID, profile name, key, timeout, or default is hardcoded.

## Validation

Passed locally:

    make -C soft386 check-quick
    make -C soft386 pm-bridge-check queue-bridge-check net-bridge-check
    soft386/pm-bridge-check
    soft386/queue-bridge-check
    soft386/net-bridge-check

The existing PM bridge profile tests still pass.  This environment does not
contain a 32-bit MinGW/RosBE compiler, so the Windows build remains for the
user's normal RosBE environment.

## Windows test

Enable both jar and provider diagnostics:

    set OS2_PM_TRACE=1
    soft386_os2.exe --max-cycles 0 --trace-native --run demos\\NEKO.EXE 2>neko-h2q.txt

The decisive block should look like:

    soft386: PMSHAPI.117 request ... app="..." key="..." pcb_in=...
    PMSHAPI: PrfQueryProfileData enter ... file=... app=... key=... pcb_in=...
    PMSHAPI: PrfQueryProfileData lookup chars=... value_head=...
    PMSHAPI: PrfQueryProfileData fail ...
      -- or --
    PMSHAPI: PrfQueryProfileData success bytes=... data=...
    soft386: PMSHAPI.117 response result=... pcb_out=... data=...

Do not change PMWIN rendering or Tiny386 based on H2P.  First resolve why the
binary profile initialization differs from the proven native path.
