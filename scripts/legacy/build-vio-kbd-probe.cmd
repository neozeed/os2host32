@echo off
rem M29A archaeology probe: compile/link one C/386 program that calls
rem VioWrtTTY and KbdCharIn.  Use a COMPLETE working C/386 runtime set.
rem The useful first result is os2host32 --scan, not necessarily execution.

if not "%OS2LIB%"=="" goto have_lib
if exist C:\c386\lib\OS2386.LIB set OS2LIB=C:\c386\lib
if not "%OS2LIB%"=="" goto have_lib
if exist C:\20ddk\lib\OS2386.LIB set OS2LIB=C:\20ddk\lib
if not "%OS2LIB%"=="" goto have_lib

echo Could not find OS2386.LIB.
echo Set OS2LIB to the historical import-library directory you want to probe.
exit /b 1

:have_lib
if not "%OS2CRT%"=="" goto have_crt
if exist "%OS2LIB%\LIBC.LIB" set OS2CRT=%OS2LIB%\LIBC.LIB
if not "%OS2CRT%"=="" goto have_crt
if exist C:\c386\lib\LIBC.LIB set OS2CRT=C:\c386\lib\LIBC.LIB
if not "%OS2CRT%"=="" goto have_crt

echo Could not find the C/386 runtime library required for this probe.
echo DDK-only directories may contain OS2386.LIB without LIBC.LIB.
echo Set OS2CRT to the LIBC.LIB from a complete working C/386 toolchain.
exit /b 1

:have_crt
cl386 /nologo /Gd /c vio-kbd-probe.c
if errorlevel 1 exit /b 1

>m29a-vio-kbd.lnk echo vio-kbd-probe.obj
>>m29a-vio-kbd.lnk echo vio-kbd-probe.exe
>>m29a-vio-kbd.lnk echo nul.map
>>m29a-vio-kbd.lnk echo %OS2CRT% %OS2LIB%\OS2386.LIB
>>m29a-vio-kbd.lnk echo nul.def
link386 @m29a-vio-kbd.lnk
if errorlevel 1 exit /b 1

echo.
echo Built vio-kbd-probe.exe.
echo First run: os2host32.exe --scan vio-kbd-probe.exe
