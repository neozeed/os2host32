# KBDCALLS R2 backend design

## Goal

Split the existing Win32-hosted KBDCALLS implementation along the same boundary
used by VIO R2, DOSCALLS R2 and QUECALLS R2:

```text
OS/2 application
    -> KBDCALLS.dll ABI veneer
    -> common OS/2 keyboard semantics/session
    -> backend contract
    -> Win32 console/input backend
```

The split is intentionally limited to the six KBDCALLS exports already present.
It does not add KbdOpen/KbdClose, focus APIs, code-page APIs, registration, or a
complete OS/2 keyboard-device subsystem.

## Common ownership

`common/kbd/os2_kbd.c` owns:

- the process/session `KBDINFO` state for the current default logical keyboard;
- OS/2 ABI structure initialization and result construction;
- `fbStatus` character-available semantics;
- `KbdStringIn` buffer length, CR/LF completion and backspace editing behavior;
- validation of KBDINFO length and pointer parameters;
- the six exported API-level semantic entrypoints;
- `os2_kbd_try_char()`, a nonblocking primitive intended for execution engines
  that have their own scheduler.

The common layer contains no Win32 types and never sees `HANDLE`,
`INPUT_RECORD`, console-control handlers, or pipe handles.

## Backend contract

`common/include/os2_kbd_backend.h` deliberately exposes only five operations:

- `read_event(wait)` -- consume the next host-normalized keyboard event;
- `peek_event()` -- inspect without consuming;
- `flush()` -- discard pending host input;
- `echo_bytes()` -- perform the existing line-input echo operation;
- `milliseconds()` -- timestamp source for empty/no-key result initialization.

A backend event contains only OS/2-relevant normalized fields:

- character byte;
- scan byte;
- OS/2-style shift-state bits;
- millisecond timestamp;
- an `echoable` source marker used to preserve the current native
  `KbdStringIn` console-versus-redirected behavior.

## Win32 backend ownership

`common/win32/os2_kbd_win32.c` owns only host mechanics:

- `GetStdHandle` / `GetFileType`;
- `ReadConsoleInputA` / `PeekConsoleInputA`;
- `GetNumberOfConsoleInputEvents`;
- `PeekNamedPipe` / `ReadFile` for redirected input;
- `FlushConsoleInputBuffer`;
- `WriteConsoleA` for the existing line-input echo path;
- mapping Win32 modifier flags to the current OS/2 `fsState` bits;
- `SetConsoleCtrlHandler` and the synthetic Ctrl+C key record used by CMD;
- `GetTickCount`;
- Win32 error-to-APIRET mapping.

No OS/2 keyboard status object is stored in the Win32 backend.

## Future WHP backend

WHP should not inherit Win32 blocking or console handles.  It can provide a
backend that returns guest keyboard events from the emulator/input source.
`os2_kbd_try_char()` allows the guest scheduler to attempt a nonblocking read,
park an OS/2 guest thread when empty, and retry it on an input wakeup.

Guest addresses are not part of this KBD common API: the WHP import veneer is
responsible for copying `KBDKEYINFO`, `STRINGINBUF`, and character buffers
between guest memory and these host-independent structures.

## Future OS2SS backend

OS2SS can expose the same common semantics while implementing input acquisition
through its subsystem console/session machinery.  A blocking application request
need not block the OS2SS server thread: the server can use a nonblocking common
read and complete/defer the client request when its console/input source signals.

## Preserved approximations

This refactor does not silently broaden behavior:

- `hkbd` continues to be ignored as in the previous native implementation;
- only the default logical keyboard is effectively supported;
- `KbdSetStatus`/`KbdGetStatus` preserve the 10-byte status object, but the
  stored mode bits do not yet reconfigure Win32 input filtering;
- `KbdStringIn` remains the conservative existing subset: printable byte input,
  CR/LF completion, Backspace editing, and the existing console echo behavior;
- the current Ctrl+C survival behavior for CMD remains Win32-backend-specific;
- no DBCS/interim-character engine or full binary/ASCII mode policy is added.

Those are future semantic milestones, not prerequisites for the backend split.
