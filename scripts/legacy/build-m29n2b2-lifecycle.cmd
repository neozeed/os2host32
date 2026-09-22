@echo off
rem M29N2b.2: real 32-bit OS/2 DLL initialization/termination lifecycle.
rem
rem M29N2C -> M29N2D proves dependency INIT and TERM ordering.
rem M29N2F deliberately returns zero from INIT to test rc=295 rollback.
rem
rem Run "make" first: the host build creates m29n2b2-setentry.exe, a small
rem fixture helper which supplies the historical SETENTRY/DLLINIT header work
rem missing from this recovered prerelease C/386 package.

if not exist m29n2b2-setentry.exe (
  echo m29n2b2-setentry.exe is missing. Run make first.
  exit /b 1
)

if not "%OS2LIB%"=="" goto have_lib
if exist C:\20ddk\lib\OS2386.LIB set OS2LIB=C:\20ddk\lib
if not "%OS2LIB%"=="" goto have_lib
if exist C:\c386\lib\OS2386.LIB set OS2LIB=C:\c386\lib
if not "%OS2LIB%"=="" goto have_lib

echo Could not find OS2386.LIB.
exit /b 1

:have_lib
echo Using OS/2 libraries from %OS2LIB%
echo Compiling CRT-free lifecycle DLLs...
cl386 /nologo /Gd /Gs /Zl /c m29n2d.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /Gs /Zl /c m29n2c.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /Gs /Zl /c m29n2f.c
if errorlevel 1 exit /b 1

>m29n2d.lnk echo m29n2d.obj
>>m29n2d.lnk echo M29N2D.DLL
>>m29n2d.lnk echo m29n2d.map
>>m29n2d.lnk echo.
>>m29n2d.lnk echo m29n2d.def
link386 @m29n2d.lnk
if errorlevel 1 exit /b 1

>m29n2c.lnk echo m29n2c.obj
>>m29n2c.lnk echo M29N2C.DLL
>>m29n2c.lnk echo m29n2c.map
>>m29n2c.lnk echo.
>>m29n2c.lnk echo m29n2c.def
link386 @m29n2c.lnk
if errorlevel 1 exit /b 1

>m29n2f.lnk echo m29n2f.obj
>>m29n2f.lnk echo M29N2F.DLL
>>m29n2f.lnk echo m29n2f.map
>>m29n2f.lnk echo.
>>m29n2f.lnk echo m29n2f.def
link386 @m29n2f.lnk
if errorlevel 1 exit /b 1

rem Ordinal 2 is each fixture's lifecycle entry.  This is the equivalent of
rem Microsoft's documented SETENTRY object: make it the LE entry point and set
rem per-process INIT/TERM header flags.
m29n2b2-setentry.exe M29N2D.DLL 2
if errorlevel 1 exit /b 1
m29n2b2-setentry.exe M29N2C.DLL 2
if errorlevel 1 exit /b 1
m29n2b2-setentry.exe M29N2F.DLL 2
if errorlevel 1 exit /b 1

if not exist m29n2b2-dll mkdir m29n2b2-dll
copy /Y M29N2C.DLL m29n2b2-dll\M29N2C.DLL >nul
copy /Y M29N2D.DLL m29n2b2-dll\M29N2D.DLL >nul
copy /Y M29N2F.DLL m29n2b2-dll\M29N2F.DLL >nul

echo Compiling dynamic lifecycle caller...
cl386 /nologo /Gd /c m29n2b2-dynamic.c
if errorlevel 1 exit /b 1
>m29n2b2-dynamic.lnk echo m29n2b2-dynamic.obj
>>m29n2b2-dynamic.lnk echo m29n2b2-dynamic.exe
>>m29n2b2-dynamic.lnk echo m29n2b2-dynamic.map
>>m29n2b2-dynamic.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29n2b2-dynamic.lnk echo m29n2b2-dynamic.def
link386 @m29n2b2-dynamic.lnk
if errorlevel 1 exit /b 1

echo Compiling load-time/process-exit lifecycle caller...
cl386 /nologo /Gd /c m29n2b2-static.c
if errorlevel 1 exit /b 1
>m29n2b2-static.lnk echo m29n2b2-static.obj
>>m29n2b2-static.lnk echo m29n2b2-static.exe
>>m29n2b2-static.lnk echo m29n2b2-static.map
>>m29n2b2-static.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29n2b2-static.lnk echo m29n2b2-static.def
link386 @m29n2b2-static.lnk
if errorlevel 1 exit /b 1

echo.
echo M29N2b.2 lifecycle fixtures built.
echo set OS2LIBPATH=m29n2b2-dll;.
echo set OS2_TRACE_MODULES=1
echo os2host32 --scan m29n2b2-dll\M29N2C.DLL
echo os2host32 --scan m29n2b2-dll\M29N2D.DLL
echo os2host32 --run m29n2b2-dynamic.exe
echo os2host32 --run m29n2b2-static.exe
