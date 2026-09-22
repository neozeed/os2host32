@echo off
rem M31C R1 - Microsoft/IBM OS/2 2.0 Beta 2 HANOI sample
make clean
if errorlevel 1 goto fail
make tools compat
if errorlevel 1 goto fail
make m31c-hanoi-check
if errorlevel 1 goto fail
os2host32.exe --scan examples\m31c-hanoi\HANOI.EXE
if errorlevel 1 goto fail
set OS2_PM_TRACE=1
os2host32.exe --run examples\m31c-hanoi\HANOI.EXE
exit /b %errorlevel%
:fail
echo M31C R1 build/check FAILED
exit /b 1
