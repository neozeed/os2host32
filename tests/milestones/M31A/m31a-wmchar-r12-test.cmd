@echo off
setlocal
set OS2_PM_TRACE=1
set OS2_PM_BYPASS_WM_CREATE=

echo === R12 stack-layout regression ===
python tools\check_m31a_wmchar_stack.py examples\m31a-wmchar\WMCHAR.EXE
if errorlevel 1 goto :fail

echo.
echo === R12 WMCHAR run ===
echo Expected startup: M31A PM stack ... 00002C80 -^> 00040000
os2host32.exe --run WMCHAR.EXE
exit /b %errorlevel%

:fail
echo R12 regression FAILED.
exit /b %errorlevel%
