# Milestone 28A3 — direct OS/2 backend CRT-decoration isolation

This is a correction to M28A2 after testing with the recovered Microsoft
C/386 6.00.081 toolchain.

The M28A2 link proved that `os2.h` + `OS2386.LIB` are now doing their job:
none of the `Dos*` names were unresolved.  The remaining failures were all
ordinary CRT string/memory helpers.  The object files requested decorated
names such as:

```
_strcpy@8
_memcpy@12
_strlen@4
_memset@12
```

while the OS/2 `LIBC.LIB` exports the cdecl forms (`_strcpy`, `_memcpy`,
`_strlen`, `_memset`).  This is a calling-convention/toolchain-header mismatch,
not a missing-library problem.

M28A3 deliberately does **not** pretend that problem is solved by `/Gd`.
For the small direct-OS/2 backend and smoke program it uses private C89 string
and memory helpers, so the smoke test has no dependency on those mismatched
CRT entry points.  This isolates the question we actually want to answer now:
can a C/386-built LX program import and call the real OS/2 DOSCALLS API through
`OS2386.LIB` and then run under OS2HOST32?

Build:

```
build-os2-backend.cmd
```

Run:

```
os2host32.exe --run cmdos2_os2_smoke.exe
```

Expected final marker:

```
M28A3_OS2_BACKEND_OK
```

The full CMD conversion should not yet use this as an excuse to duplicate the
whole CRT.  Once the DOSCALLS smoke passes, the next toolchain task is to pin
down why the recovered C/386 driver/header combination decorates ordinary CRT
calls as stdcall-style names while the selected OS/2 LIBC is cdecl.  Until
then, the OS/2 API ABI and the CRT ABI are kept as two separate problems.
