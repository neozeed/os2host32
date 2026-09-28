# os2host32 — KBDCALLS R2 handoff
## Backend-neutral keyboard semantics/state with Win32 input backend

Date: 2026-09-27

Status:

`KBD_R2_COMMON_STATE_WIN32_BACKEND_STATIC_VERIFIED_RUNTIME_PENDING`

## Baseline

KBD R2 was made from `os2host32-QUECALLS-R2.zip`:

SHA-256:

`0ffab8b3a9c869b3d6b78dd447781bc1cd550462822b70e1aa368aa9d5b03921`

The user has subsequently confirmed the QUECALLS split worked correctly in
Sarien.  The DOSCALLS/VIO R2B baseline was already live-proven with all demos.

## Objective

Give KBDCALLS the same architecture as VIO, DOSCALLS and QUECALLS so future
WHP and ReactOS OS2SS personalities can supply their own input backends without
copying Win32 keyboard semantics.

No new KBD API was added.

## Frozen ABI

The existing `dlls/kbdcalls/kbdcalls.def` is byte-identical, SHA-256:

`46e62bde47fa01017fcaf929a6c881720dbcc08649bb2dc30b376da05aa9a7ab`

Exports remain:

- KbdCharIn @4
- KbdStringIn @9
- KbdGetStatus @10
- KbdSetStatus @11
- KbdFlushBuffer @13
- KbdPeek @22

The loader was not changed.  Its SHA-256 remains:

`793ee7de7c6d0bd79b7d8bbdfaab6774cb2ec6af39b88d0cefc3c714430f0c55`

Therefore all existing C/386 far16 KBD descriptors remain exactly as before.

## New architecture

```text
KBDCALLS.dll
  dlls/kbdcalls/kbdcalls.c
        |
        v
common/kbd/os2_kbd.c
        |
        v
Os2KbdBackendOps
        |
        v
common/win32/os2_kbd_win32.c
```

### Common layer owns

- default logical-keyboard KBDINFO state;
- pointer/length validation;
- KBDKEYINFO initialization/result formation;
- fbStatus=0x40 availability result;
- KbdStringIn byte collection, CR/LF termination and Backspace handling;
- current buffer length rules;
- KbdGetStatus/KbdSetStatus state;
- scheduler-friendly `os2_kbd_try_char()`.

### Win32 backend owns

- console INPUT_RECORD acquisition and peek;
- redirected pipe/file bytes;
- console-control handler used to keep CMD alive across Ctrl+C/Ctrl+Break;
- synthetic Ctrl+C KEY_EVENT injection while a KBD read is waiting;
- Win32 virtual scan code and modifier-state translation;
- FlushConsoleInputBuffer;
- WriteConsoleA line-input echo;
- GetTickCount;
- Win32 error mapping.

There are no Windows headers/types in common KBD semantics and no Windows calls
in the public KBDCALLS veneer.

## Compatibility choices

This is an architectural split, not a KBD semantic expansion.

The following pre-existing behavior is deliberately retained:

- `hkbd` is ignored; effectively only the default logical keyboard exists;
- KbdSetStatus stores KBDINFO but does not reconfigure Win32 input modes;
- KbdStringIn implements the same conservative line-input subset;
- console input is echoed on the existing KbdStringIn path;
- redirected input remains a byte stream and is not console-echoed;
- KbdCharIn itself does not echo;
- KbdPeek does not consume;
- Ctrl+C handling remains a Win32-backend concern.

## WHP/OS2SS seam

`os2_kbd_try_char()` provides a nonblocking common semantic operation.  A WHP
backend can feed guest keyboard events and let the guest scheduler park/wake a
thread without `ReadConsoleInputA`.  OS2SS can similarly defer a client request
until subsystem console/input state becomes ready rather than blocking its
server thread.

Neither backend is implemented here.

## Verification

Passed under the host GCC toolchain:

- kbd-core-check
- kbd-veneer-check
- kbd-win32-shim-check
- KBD-R2 static architecture check
- complete pre-existing `make verify`

The same complete `make verify` passes under Clang 17.

Tests cover:

- empty IO_NOWAIT;
- WAIT event delivery;
- character, scan, modifier state and timestamp propagation;
- non-consuming KbdPeek;
- common KBDINFO set/get state;
- KBDINFO invalid length;
- KbdStringIn CR/backspace/buffer behavior;
- console echo isolation;
- redirected pipe input;
- KbdFlushBuffer backend routing;
- `os2_kbd_try_char()` nonblocking behavior;
- actual public veneer routing;
- Win32 input-record adapter behavior using a deterministic stub.

The MinGW i686 compiler is unavailable in the build environment, so the real
`KBDCALLS.dll` MinGW build and Windows runtime are intentionally not claimed.

## Windows build/test

First:

```text
make clean
make KBDCALLS.dll
```

Then:

```text
make all
make verify
```

Recommended runtime regression:

1. `cmd32os2.exe` -- ordinary editing, command entry, Ctrl+C behavior;
2. `lifeos2.exe` -- IO_NOWAIT, arrows, Insert/Delete, ESC;
3. C/386 far16 `KbdCharIn`, `KbdStringIn`, and `KbdFlushBuffer` probes;
4. the usual demo set that previously passed on DOSCALLS/VIO R2B.

If those pass, promote this milestone to:

`KBD_R2_COMMON_STATE_WIN32_BACKEND_LIVE_PASS`

## Do not broaden the next KBD milestone accidentally

Reasonable follow-on work is to make the stored KBDINFO mode genuinely affect
common keyboard filtering/echo rules, or to add missing logical-keyboard APIs.
Do not mix that with a WHP/OS2SS backend implementation until the common
semantics are independently stable.
