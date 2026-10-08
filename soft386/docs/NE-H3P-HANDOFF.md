# NE-H3P: 16-bit MSG.2 wrapper preservation and diagnostic

This is an *investigation checkpoint*, not a completed MSG.2 service. It preserves the original OS/2 1.2 OS2.LIB and documents its `DOSGETMESSAGE` -> `DOSTRUEGETMESSAGE` thunk in `docs/reference/16bitdiscovery/`.

The only runtime change is a read-only 32-word stack dump when an NE program reaches unresolved `MSG.2`; the normal explicit unsupported-call stop remains in place. No premature stack pop, guest buffer write, fake success, or system DLL modification occurs. Earlier NE imports, 32-bit execution, and guest memory semantics are not changed.

Reproduce on Windows:

```bat
soft386_os2.exe --trace-hc --run \cl386-research\os2_2.0\x\SDK20\TOOLKT20\OS2BIN\link386.exe huge.obj > link-h3p.log 2>&1
```

Use the `MSG.2 RAW FRAME` lines to identify arguments alongside the original `OS2.LIB` wrapper and `MSG.DLL` implementation before implementing the adapter. The linker is currently expected to stop with an explicit unsupported MSG.2; it is **not** yet expected to write nonzero bytes to HUGE.EXE.
