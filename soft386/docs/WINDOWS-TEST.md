# Soft386 R0 Windows test sequence

Build from the repository root with an i686 MinGW-w64 toolchain:

```bat
make soft386-win32
```

If your native MinGW compiler is named `gcc` rather than
`i686-w64-mingw32-gcc`:

```bat
cd soft386
make WINCC=gcc win32
```

Run the packaged regression images:

```bat
cd soft386
soft386_os2.exe --check tests\fixtures\hello-soft386.le
soft386_os2.exe tests\fixtures\hello-soft386.le
soft386_os2.exe tests\fixtures\hello-soft386.lx
soft386_os2.exe tests\fixtures\internal-soft386.le
soft386_os2.exe tests\fixtures\internal-soft386.lx
soft386_os2.exe tests\fixtures\hi-surface-soft386.le
soft386_os2.exe --trace-hc tests\fixtures\thread-soft386.le
```

Expected guest text includes:

```text
soft386: untouched 32-bit OS/2 LE/LX says hello!
soft386: internal OFF32 fixup PASS
soft386: historical hi.exe DOSCALLS surface PASS
soft386: worker thread ran
soft386: main resumed after DosWaitThread
```

The thread trace should show a switch to TID 2 with FS=0020 and restoration of
TID 1 with FS=0018.

## Historical hi.exe

For the known historical fixture whose SHA256 is
`3e0383860d8e76262b3c954bb95ae8e8e3c7887deddfbfd7705c4b8c580441c7`:

```bat
soft386_os2.exe --check hi.exe
soft386_os2.exe --trace-hc hi.exe
```

R0 already routes its known DOSCALLS ordinal surface
224/234/256/282/299/304/305/348.  A runtime failure beyond that point should
be treated as useful evidence: preserve the complete trace and the image hash.
