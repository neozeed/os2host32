@echo off
rem M29N2a: build two CRT-free C/386 OS/2 DLLs and one normal C/386 EXE.
rem The dependency graph is:
rem
rem   m29n2a-import.exe --name--> M29N2A.DLL --ordinal 1--> M29N2B.DLL
rem
rem The DLL objects use /Zl and /Gs so this first loader test has no C runtime
rem startup dependency.  DLL init/term belongs to M29N2b.

if not "%OS2LIB%"=="" goto have_lib
if exist C:\20ddk\lib\OS2386.LIB set OS2LIB=C:\20ddk\lib
if not "%OS2LIB%"=="" goto have_lib
if exist C:\c386\lib\OS2386.LIB set OS2LIB=C:\c386\lib
if not "%OS2LIB%"=="" goto have_lib

echo Could not find OS2386.LIB.
echo Set OS2LIB to the directory containing LIBC.LIB and OS2386.LIB.
exit /b 1

:have_lib
echo Using OS/2 libraries from %OS2LIB%

echo Compiling CRT-free guest DLL objects...
cl386 /nologo /Gd /Gs /Zl /c m29n2b.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /Gs /Zl /c m29n2a.c
if errorlevel 1 exit /b 1

>m29n2b.lnk echo m29n2b.obj
>>m29n2b.lnk echo M29N2B.DLL
>>m29n2b.lnk echo m29n2b.map
>>m29n2b.lnk echo.
>>m29n2b.lnk echo m29n2b.def
link386 @m29n2b.lnk
if errorlevel 1 exit /b 1

>m29n2a.lnk echo m29n2a.obj
>>m29n2a.lnk echo M29N2A.DLL
>>m29n2a.lnk echo m29n2a.map
>>m29n2a.lnk echo.
>>m29n2a.lnk echo m29n2a.def
link386 @m29n2a.lnk
if errorlevel 1 exit /b 1

echo Compiling normal C/386 caller EXE...
cl386 /nologo /Gd /c m29n2a-import.c
if errorlevel 1 exit /b 1

>m29n2a-import.lnk echo m29n2a-import.obj
>>m29n2a-import.lnk echo m29n2a-import.exe
>>m29n2a-import.lnk echo m29n2a-import.map
>>m29n2a-import.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29n2a-import.lnk echo m29n2a-import.def
link386 @m29n2a-import.lnk
if errorlevel 1 exit /b 1

if not exist m29n2a-dll mkdir m29n2a-dll
copy /Y M29N2A.DLL m29n2a-dll\M29N2A.DLL >nul
if errorlevel 1 exit /b 1
copy /Y M29N2B.DLL m29n2a-dll\M29N2B.DLL >nul
if errorlevel 1 exit /b 1

echo.
echo M29N2a DLL fixtures built.
echo Scan A: os2host32 --scan m29n2a-dll\M29N2A.DLL
echo Scan B: os2host32 --scan m29n2a-dll\M29N2B.DLL
echo Direct: set OS2LIBPATH=m29n2a-dll;.
echo         set OS2_TRACE_MODULES=1
echo         os2host32 --run m29n2a-import.exe
