Portable ReactOS SDK subset for OS2SS
=====================================

This directory is intentionally a small, frozen subset of the ReactOS i386
build/source tree.  Normal OS2SS builds must not require a complete ReactOS
checkout.

Expected layout:

  reactos/
    psdk/                 generated/public SDK headers (winnt.h, ntdef.h, ...)
    ddk/                  generated DDK headers (wdm.h, ntddk.h, ...)
    ndk/                  ReactOS NDK headers
    reactos/              ReactOS-private include tree
      subsys/sm/          SMSS protocol headers
    lib/
      libntdll.a          i386 ReactOS import library
      libkernel32.a       i386 ReactOS import library
      libsmlib.a          i386 ReactOS SM client library

The header and library copies should all come from the same frozen ReactOS
revision/build.  The current OS2SS milestone was developed against:

  091855fc4f9de8052c8cf4a55830580aab5558da

The canonical compiler constraint for this portable build is:

  i686-w64-mingw32-gcc

The old build-le4c-canonical.sh remains in the tree for provenance/regression
work and is not deleted or rewritten by this Makefile conversion.
