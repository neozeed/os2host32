@echo off
rem M29N2b.1: build the existing CRT-free A->B DLL chain and a caller that
rem discovers it entirely at runtime via DosLoadModule/DosQueryProcAddr.

call build-m29n2a-dll.cmd
if errorlevel 1 exit /b 1

if not "%OS2LIB%"=="" goto have_lib
if exist C:\20ddk\lib\OS2386.LIB set OS2LIB=C:\20ddk\lib
if not "%OS2LIB%"=="" goto have_lib
if exist C:\c386\lib\OS2386.LIB set OS2LIB=C:\c386\lib
if not "%OS2LIB%"=="" goto have_lib

echo Could not find OS2386.LIB.
exit /b 1

:have_lib
echo Compiling M29N2b.1 dynamic module API caller...
cl386 /nologo /Gd /c m29n2b1-dynamic.c
if errorlevel 1 exit /b 1

>m29n2b1-dynamic.lnk echo m29n2b1-dynamic.obj
>>m29n2b1-dynamic.lnk echo m29n2b1-dynamic.exe
>>m29n2b1-dynamic.lnk echo m29n2b1-dynamic.map
>>m29n2b1-dynamic.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29n2b1-dynamic.lnk echo m29n2b1-dynamic.def
link386 @m29n2b1-dynamic.lnk
if errorlevel 1 exit /b 1

echo.
echo M29N2b.1 fixture built.
echo Scan:   os2host32 --scan m29n2b1-dynamic.exe
echo Direct: set OS2LIBPATH=m29n2a-dll;.
echo         set OS2_TRACE_MODULES=1
echo         os2host32 --run m29n2b1-dynamic.exe
