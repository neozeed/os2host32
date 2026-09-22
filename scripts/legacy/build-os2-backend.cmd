@echo off
rem M29I direct OS/2 C/386 LE/LX backend build.
rem
rem The backend now uses genuine C/386 _far16 _pascal VIO/KBD migration
rem helpers for VioScrollUp, VioGetCurPos, VioSetCurPos, VioWrtTTY, VioGetMode,
rem KbdCharIn and KbdFlushBuffer.  The DEF files below
rem provide the classic ordinal imports plus DosFlatToSel required by those
rem compiler-generated helpers.
rem
rem The smoke path intentionally avoids ordinary CRT string/memory calls because
rem this recovered C/386/header combination decorates them incompatibly with
rem the OS/2 LIBC import library.  os2.h still supplies the real Dos* ABI.
rem
rem Set OS2LIB if your LIBC.LIB + OS2386.LIB live somewhere else.

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

cl386 /nologo /Gd /c cmdos2_env.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c cmdos2_os2.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c cmdos2_os2_smoke.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c cmdos2_os2_console_test.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c cmdos2_os2_find_test.c
if errorlevel 1 exit /b 1

cl386 /nologo /Gd /c env-child.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c handle-child.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c async-child.c
if errorlevel 1 exit /b 1

>m28b-child.lnk echo env-child.obj
>>m28b-child.lnk echo env-child.exe
>>m28b-child.lnk echo nul.map
>>m28b-child.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m28b-child.lnk echo nul.def
link386 @m28b-child.lnk
if errorlevel 1 exit /b 1

>m28c-handle-child.lnk echo handle-child.obj
>>m28c-handle-child.lnk echo handle-child.exe
>>m28c-handle-child.lnk echo nul.map
>>m28c-handle-child.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m28c-handle-child.lnk echo nul.def
link386 @m28c-handle-child.lnk
if errorlevel 1 exit /b 1

>m28d-async-child.lnk echo async-child.obj
>>m28d-async-child.lnk echo async-child.exe
>>m28d-async-child.lnk echo nul.map
>>m28d-async-child.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m28d-async-child.lnk echo nul.def
link386 @m28d-async-child.lnk
if errorlevel 1 exit /b 1

>m28b-smoke.lnk echo cmdos2_os2_smoke.obj cmdos2_os2.obj cmdos2_env.obj
>>m28b-smoke.lnk echo cmdos2_os2_smoke.exe
>>m28b-smoke.lnk echo nul.map
>>m28b-smoke.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m28b-smoke.lnk echo cmdos2_os2_smoke.def

link386 @m28b-smoke.lnk
if errorlevel 1 exit /b 1

>m29h-console.lnk echo cmdos2_os2_console_test.obj cmdos2_os2.obj cmdos2_env.obj
>>m29h-console.lnk echo cmdos2_os2_console_test.exe
>>m29h-console.lnk echo nul.map
>>m29h-console.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29h-console.lnk echo cmdos2_os2_console_test.def
link386 @m29h-console.lnk
if errorlevel 1 exit /b 1

>m29i-find.lnk echo cmdos2_os2_find_test.obj cmdos2_os2.obj cmdos2_env.obj
>>m29i-find.lnk echo cmdos2_os2_find_test.exe
>>m29i-find.lnk echo nul.map
>>m29i-find.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29i-find.lnk echo cmdos2_os2_find_test.def
link386 @m29i-find.lnk
if errorlevel 1 exit /b 1

echo.
echo Built cmdos2_os2_smoke.exe with the M29H complete VIO/KBD far16 console backend.
echo Run it through: os2host32.exe --run cmdos2_os2_smoke.exe
echo.
echo Built cmdos2_os2_console_test.exe as the M29H interactive backend probe.
echo Scan it first: os2host32.exe --scan cmdos2_os2_console_test.exe
echo Then run it:  os2host32.exe --run cmdos2_os2_console_test.exe
echo.
echo Built cmdos2_os2_find_test.exe as the M29I DosFindFirst/DosFindNext probe.
echo Run it:       os2host32.exe --run cmdos2_os2_find_test.exe
