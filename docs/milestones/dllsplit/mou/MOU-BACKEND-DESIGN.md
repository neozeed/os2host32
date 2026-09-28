# OS2HOST32 MOUCALLS R2 backend design

## Purpose

MOUCALLS did not exist in the native tree before this milestone.  Rather than
introduce a Win32-owned mouse DLL and immediately refactor it, R2 starts with
the same architecture already proven for VIO/KBD/QUEUE/SESMGR/NLS:

    MOUCALLS.dll ABI veneer
            -> common/mou/os2_mou.c
            -> Os2MouBackendOps
            -> common/win32/os2_mou_win32.c

The common layer is the OS/2 personality.  The Win32 layer is only one source
of host pointing-device observations.

## Common-owned state

`Os2MouSession` owns:

- HMOU allocation/lifetime;
- current event mask;
- OS/2 device-status flags;
- pointer position;
- queue of translated `MOUEVENTINFO` records;
- button and mickey counts exposed to the guest;
- scale factors and acceleration threshold state;
- logical pointer shape bytes/metadata;
- draw/remove/exclusion state.

The event queue contains OS/2 structures, not Win32 `INPUT_RECORD` values.
`os2_mou_try_read()` is the nonblocking semantic primitive intended for WHP or
OS2SS schedulers.

## Win32 backend

The first backend deliberately does **not** read the Win32 console input
record stream.  KBDCALLS already consumes that stream and the two DLLs cannot
share a normal C static queue.  Two independent `ReadConsoleInputA` consumers
would lose each other's keyboard/mouse events.

Instead the Win32 backend samples:

- `GetCursorPos` for the host pointer;
- `GetAsyncKeyState` for the first three buttons;
- console window/font/buffer metrics to translate host pixels to text cells;
- `SetCursorPos` for best-effort `MouSetPtrPos`.

This polling backend is intentionally a native-host approximation.  It avoids
keyboard interference and keeps all OS/2 event filtering/queue semantics in
common code.  A future WHP backend can inject guest mouse events directly and
an OS2SS backend can source events from its own console/session infrastructure.

## Important approximations

- Win32 does not expose the OS/2 "mickeys per centimetre" quantity; the backend
  reports a deterministic value of 8.
- Host polling can collapse multiple physical movements between samples into a
  single OS/2 event.
- `MouSetScaleFact` and threshold values are common personality state; this
  backend does not reprogram Windows mouse acceleration.
- Pointer shape/draw/exclusion state is maintained logically.  The Win32
  console backend does not replace the host GUI pointer image.
- `GetConsoleWindow`/font based cell mapping is intended for the traditional
  Win32 console host; pseudoconsole/Windows Terminal geometry may be less exact.
- `MouRegister`/`MouDeRegister` router replacement is explicitly unsupported.

## API surface

The historical ordinal surface from `sdk/os2h/bseord.h` is exported, excluding
unused ordinal holes 5 and 12.  Normal device/event APIs are implemented in
common semantics.  `MouRegister` and `MouDeRegister` return the historical
registration/deregistration error values instead of pretending to work.
