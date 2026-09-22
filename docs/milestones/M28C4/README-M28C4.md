# Milestone 28C4 — MinGW redirection build + deterministic low LX mapping

M28C4 is a narrow correction to M28C3.

## 1. MinGW `_S_IREAD` / `_S_IWRITE`

`cmdos2_win32.c` now includes `<sys/stat.h>`.  The native redirection backend
uses `_S_IREAD | _S_IWRITE` as the creation mode passed to `_open()`, and
MinGW declares those mode bits in that header.

The unused `contains_wildcard()` helper was also removed, eliminating the
warning seen in the M28C3 build.

## 2. Fixed-address LX mapping

The direct LX execution model deliberately maps C/386 objects at their original
linear addresses (typically 0x00010000, 0x00020000, ...).  A Win32 host built
with ASLR enabled can occasionally consume or collide with those low addresses,
which explains a run that fails before the first `mapped object` line with:

    os2host32: cannot map LX object at requested linear address

M28C4 therefore does two things:

* links `os2host32.exe` at fixed image base 0x00400000 with dynamic-base ASLR
  disabled;
* reserves 0x00010000..0x003FFFFF as a guest window at process startup, before
  loading/parsing the LX file, then commits each LX object inside that reserve.

This is intentionally a low-address compatibility arena for the historical
C/386 binaries.  Objects outside it still use the previous exact-address
`VirtualAlloc` path.

If a requested address is ever unavailable, OS2HOST32 now prints the relevant
`VirtualQuery` state/type/allocation base before failing, which should make the
next collision diagnosable instead of mysterious.

## Regression

Native shell:

    make
    cd examples
    ..\cmd32os2.exe
    m28c-redir-test.cmd

Expected tail:

    delete-one=OK
    delete-two=OK
    M28C4 redirection regression complete

The genuine LX M28C2/M28C3 smoke path is otherwise unchanged and should still
finish with:

    pipe child exact payload=OK (20 bytes)
    stdout redirection/append through DosDupHandle=OK
    M28C_OS2_BACKEND_OK
