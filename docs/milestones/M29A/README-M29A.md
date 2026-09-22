# M29A — first VIOCALLS/KBDCALLS console migration

M29A starts the console side of the OS/2 personality.  The immediate goal is
not to emulate every historical VIO/KBD function.  It is to remove the native
Win32 console dependency from CMD proper and establish the same boundary that a
future genuine OS/2 CMD image will use.

## Native bootstrap path

`cmd32os2.exe` now loads three personality modules at startup:

```text
DOSCALLS.dll   filesystem / process / HFILE services
VIOCALLS.dll   video/console services
KBDCALLS.dll   keyboard services
```

The first host implementations use the historical ordinals recovered from the
old CMD image.  M29A implements only the subset needed by the current shell:

```text
KBDCALLS.4   KbdCharIn
KBDCALLS.13  KbdFlushBuffer

VIOCALLS.7   VioScrollUp
VIOCALLS.9   VioGetCurPos
VIOCALLS.15  VioSetCurPos
VIOCALLS.19  VioWrtTTY
VIOCALLS.21  VioGetMode
```

`cmd32os2.c` therefore no longer contains direct calls to `GetStdHandle`,
`ReadConsole*`, `WriteConsole*`, `FillConsole*`, `SetConsole*`, or
`ScrollConsole*`.  Win32 remains an implementation detail of VIOCALLS/KBDCALLS.

The following shell behavior now crosses the OS/2-facing boundary:

```text
interactive prompt       -> VioWrtTTY
interactive line input   -> KbdCharIn + VioWrtTTY echo
PAUSE                    -> VioWrtTTY + KbdFlushBuffer + KbdCharIn
CLS                      -> VioGetMode + VioScrollUp + VioSetCurPos
```

The line reader handles ordinary characters, CR/LF, backspace and Ctrl-Z at an
empty prompt.  Extended keys are ignored for this first milestone.

## Genuine C/386 backend

There is one deliberate temporary split.  The direct C/386 backend remains
buildable using `DosRead`/`DosWrite` fallbacks for its console abstraction.
The historical VIO/KBD libraries are expected to expose compatibility/thunked
entry points whose exact fixup form still needs to be captured before the
direct LE/LX loader can call them safely.

That is the purpose of `vio-kbd-probe.c`.  Build it with one complete working
C/386 SDK/library set, then inspect the executable before trying to run it:

```cmd
build-vio-kbd-probe.cmd
os2host32.exe --scan vio-kbd-probe.exe
```

The import/fixup inventory will define M29B: add exactly the selector/alias or
thunk form emitted by C/386 for KBD/VIO instead of guessing it.

## Native regression

Build the normal Win32 bootstrap and compatibility DLLs:

```cmd
make
cd examples
..\cmd32os2.exe
```

At the shell prompt run:

```cmd
m29a-console-test.cmd
```

Expected behavior:

1. `PAUSE` prints its prompt and waits for one key.
2. After a key, the script continues.
3. `CLS` clears the visible console and returns the cursor to the top-left.
4. The final `M29A VIO/KBD console regression complete` line is visible.

Also type a few commands manually and exercise backspace.  This is useful
because the interactive command line now comes from KBDCALLS rather than
`fgets(stdin)`.

## Scope deliberately deferred

The old CMD also used more of the VIO/KBD families: keyboard status/string/code
page calls and VIO ANSI, mode, cursor type, code page, configuration and
attribute-output calls.  Those can be added incrementally once the basic
boundary and the direct-loader thunk contract are proven.
