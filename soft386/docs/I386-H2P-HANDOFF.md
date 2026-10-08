# Soft386 I386-H2P — guest-memory watchpoints

## Parent
H2O bounded WM_TIMER CPU trace.

## Why H2P exists
The live H2O Neko trace proved the first three OS/2 WM_TIMER callbacks execute the exact same 101-instruction EIP sequence under Tiny386. The timer dispatcher reads word state at guest 0x00020074. It is 2 on every observed tick, causing the callback to zero 0x0002007C and restart the same idle animation path. Native os2host32 eventually leaves that path and reaches WinQueryWindowPos/WinSetWindowPos.

The important decoded sequence is:

    0001083A  movzx eax,word [00020074]
    ...
    00010845  je    00010855        ; state == 2
    00010855  mov   word [0002007C],0
    ...
    00010987  inc   word [0002007C]

H2P does not change PM semantics or Tiny386 execution. It adds a generic guest-memory watchpoint so the exact instruction or hostcall changing this state block can be identified.

## New diagnostic option

    --watch-mem ADDR:LEN

- ADDR and LEN accept C numeric syntax (`0x...` or decimal).
- LEN must be 1..256 bytes.
- Range must lie inside guest RAM.
- Around each Tiny386 step, changed bytes are printed with the pre-instruction EIP.
- Hostcall copy-back changes are also checked separately.
- The facility is generic; there is no Neko executable-name or fixed-address special case in runtime behavior.

Example output:

    soft386: mem-watch cpu EIP=00010987 addr=0002007C size=1 old=00 new=01

## Neko acceptance / diagnosis run

    soft386_os2.exe --max-cycles 0 --trace-native --watch-mem 0x20068:0x20 --run demos\NEKO.EXE 2>neko-h2p.txt

The range 0x20068..0x20087 covers the state selector 0x20074, animation counter 0x2007C and nearby state observed in H2O.

What matters:
1. every write touching 0x20074;
2. whether the write is made by guest CPU or hostcall copy-back;
3. the EIP and old/new bytes;
4. whether 0x20074 becomes something other than 2 and is later restored/reset.

## Validation
- `make -C soft386 check-quick`: PASS on the build host.
- Existing hello/thread/sync/memory and DOSCALLS/system bridge quick regressions pass.
- Existing Tiny386 warnings remain unchanged.

## Architecture rule
This remains diagnostic-only. Do not fix Neko by forcing a state value. Once the first incorrect write/absence is identified, fix the generic CPU/callback/marshalling cause.
