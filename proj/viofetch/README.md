# viofetch for the split os2host32 DLLs

This is the supplied neon OS/2 logo program restored as a C89, 32-bit application.
It queries version, uptime, memory, current/prepared code pages, country, current
drive and allocation information. Country is labelled as country rather than
being used to claim a keyboard layout. Global/local information-segment access
and the uninitialized fullscreen guess have been removed.

DOS queries use flat 32-bit cdecl imports. Country information imports NLS.5.
On Microsoft C/386 only the four VIO calls use explicit `_far16 _pascal`
declarations, as in the established Life bridge examples. The global INCL_16
switch is unnecessary. The narrow mode prefix and country/disk structures
match the byte layouts consumed by the compatibility DLLs.

## Microsoft C/386 OS/2 build

Run `make` in this directory with cl386 and LINK386 available and LIB pointing
at LIBC.LIB and OS2386.LIB. The link response and import definition files are
included. The default compiler flags use `/W3`; the source is C89.

Run the resulting LE executable through:

```
os2host32.exe --scan viofetch.exe
os2host32.exe --run viofetch.exe
```

This historical compiler/linker build has not been executed in this environment.
The explicit VIO declarations follow the existing proven C/386 examples.

## Native Win32 diagnostic

After building the root DLLs:

```
make -f Makefile.win32
```

Copy viofetch-pe.exe beside VIOCALLS.dll, DOSCALLS.dll and NLS.dll and run it
directly from a console. This PE build exercises the same source and APIs without
the LE loader or far16 migration bridge. It was cross-compiled successfully.

The screen needs at least 76 columns and 16 rows. Positioned text is clipped
at the visible width. Query failures appear as return codes; VIO failures cause
a nonzero exit status. The free-running millisecond counter wraps after about
49.7 days. Disk sizes avoid 32-bit byte-product overflow. Memory figures describe
the host compatibility environment and saturate at the OS/2 ULONG limit.
