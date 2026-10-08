# Soft386 NE-H3N: per-image module-reference dispatch

Baseline: NE-H3M source. Changes confined to soft386.

* The NE module-reference table is read as 1-based offsets into the NE length-prefixed import-name table. Each resolved module name is normalized to uppercase, bounds checked, and stored per NE image.
* 16-bit dispatch matches DOSCALLS and NLS by resolved module name, NOT fixed module slot (previously 1 and 3). NLS.4 return cleanup likewise matches the module name.
* `--trace-hc` prints the actual per-image module-reference table.
* Unknown calls identify the module name and ordinal, e.g. `MSG.2 (module index 4)`.
* LINK386 actual module map: 1 DOSCALLS; 2 KBDCALLS; 3 NLS; 4 MSG. `MSG.2` is NOT implemented here: no validated ordinal ABI/export available. Do not replace with an undocumented success stub.
* An automated module-map test is added to check-ne.

Verification: diagnostic (-O0) build succeeded; `make check-ne` passed before adding the extra module-map test, which passed separately. After adding it, quick regression progressed through new table test, ARGS, LINK386 startup, LIB create/add, VOID and TINY, but execution time limit interrupted the run during MEDIUM. Full quick suite is not verified for H3N. Win32 build/runtime not verified.

The linker reaches MSG.2 after opening output file, as established by the user-provided Windows H3M trace. Next: identify MSG.2 using an actual historical MSG.DLL export table or relevant SDK import library; implement its verified 16-bit ABI; test first output write on Windows with LLIBCE.LIB and OS2.LIB.
