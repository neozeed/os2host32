# KBDCALLS R2 API matrix

| API | Ordinal | Pre-R2 implementation | KBD R2 implementation | Common-owned semantics | Win32 backend role | Runtime status |
|---|---:|---|---|---|---|---|
| KbdCharIn | 4 | monolithic Win32 DLL | thin veneer -> common KBD | validate/initialize KBDKEYINFO, fbStatus, WAIT/NOWAIT request | consume normalized console/pipe event, Ctrl+C integration, scan/modifier/time mapping | static verified; live pending |
| KbdStringIn | 9 | monolithic Win32 DLL | thin veneer -> common KBD | buffer/cchIn, CR/LF completion, Backspace editing, current 255-byte cap behavior | acquire event/byte source and perform current console echo | static verified; live pending |
| KbdGetStatus | 10 | DLL static KBDINFO | thin veneer -> common KBD | validate length and return session KBDINFO | none | static verified; live pending |
| KbdSetStatus | 11 | DLL static KBDINFO | thin veneer -> common KBD | validate length and store session KBDINFO | none | static verified; live pending |
| KbdFlushBuffer | 13 | direct Win32 call | thin veneer -> common KBD | API routing | FlushConsoleInputBuffer when console input | static verified; live pending |
| KbdPeek | 22 | direct Win32/pipe peek | thin veneer -> common KBD | initialize/build KBDKEYINFO without consuming | PeekConsoleInputA/PeekNamedPipe and host event normalization | static verified; live pending |

## ABI

`dlls/kbdcalls/kbdcalls.def` remains byte-identical:

```text
LIBRARY KBDCALLS
EXPORTS
    KbdCharIn       @4 NONAME
    KbdStringIn     @9 NONAME
    KbdGetStatus    @10 NONAME
    KbdSetStatus    @11 NONAME
    KbdFlushBuffer  @13 NONAME
    KbdPeek         @22 NONAME
```

The existing C/386 far16 descriptor tables in `loader/os2host32.c` are unchanged.

## Known retained limitations

- default logical keyboard only; `hkbd` remains ignored;
- KBDINFO mode values are stored/retrieved but do not yet drive host input mode;
- conservative KbdStringIn implementation rather than full historical line editor;
- no KbdOpen/KbdClose/focus/code-page/register APIs;
- no DBCS/interim-character semantics;
- no WHP or OS2SS backend is implemented in this milestone.
