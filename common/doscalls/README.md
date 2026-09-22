# Shared DOSCALLS implementations

`os2_doscalls_core.c` owns OS/2-visible validation, fixed structure packing,
and result writeback for APIs used by both execution backends.

The code must remain independent of native process pointers and WHP internals.
Application addresses are always `os2_addr32_t` values and all access goes
through `Os2PersonalityOps`.

Backend callbacks may translate handles, perform host I/O, manage native or
guest allocations, and obtain platform data.  Calls which suspend/resume a
guest CPU context do not belong in this file.
