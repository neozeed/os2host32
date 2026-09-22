@echo off
rem OS2HOST32 M29N1 guest environment seed.
rem
rem This file is intentionally a CMD script rather than host process setup.
rem Run it inside CMD32OS2 as "env".  OS2PATH is mirrored into the guest
rem PATH variable; Win32 PATH is not the OS/2 command namespace anymore.

set OS2PATH=.;C:\OS2;C:\OS2\SYSTEM;C:\OS2\APPS

rem M29N2a consumes OS2LIBPATH when resolving genuine guest LE/LX DLLs.
rem Personality modules such as DOSCALLS remain host-backed and do not use it.
set OS2LIBPATH=.;C:\OS2\DLL

rem Historical data-file path.  Current CMD file operations do not search it.
set DPATH=.;C:\OS2;C:\OS2\SYSTEM

echo OS/2 environment initialized.
path
