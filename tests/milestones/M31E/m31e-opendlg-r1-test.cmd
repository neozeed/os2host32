@echo off
setlocal
set OS2LIBPATH=examples\m31e-opendlg;.
set OS2_PM_TRACE=1
set OS2_TRACE_MODULES=1

echo === M31E R1 OPENDLG real guest PM DLL test ===
echo.
echo [1/3] Scan OPENDLG.DLL
os2host32.exe --scan examples\m31e-opendlg\OPENDLG.DLL
if errorlevel 1 goto fail

echo.
echo [2/3] Scan HELLO.EXE
os2host32.exe --scan examples\m31e-opendlg\HELLO\HELLO.EXE
if errorlevel 1 goto fail

echo.
echo [3/3] Run HELLO.EXE with OPENDLG.DLL on OS2LIBPATH
os2host32.exe --run examples\m31e-opendlg\HELLO\HELLO.EXE
set RC=%ERRORLEVEL%
echo.
echo HELLO returned %RC%
exit /b %RC%

:fail
echo.
echo M31E R1 setup/scan failed.
exit /b 1
