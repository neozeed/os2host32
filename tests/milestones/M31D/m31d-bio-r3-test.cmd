@echo off
setlocal
set OS2_PM_TRACE=1
make m31d-bio-check
if errorlevel 1 exit /b 1
os2host32.exe --scan examples\m31d-bio\BIO.EXE
if errorlevel 1 exit /b 1
os2host32.exe --run examples\m31d-bio\BIO.EXE
endlocal
