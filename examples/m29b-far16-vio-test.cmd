@echo off
setlocal
set HOST=%~dp0..\os2host32.exe
set GUEST=%~dp0..\fixtures\c386-far16-vio.exe

echo === M29B C/386 far16 VIO bridge regression ===
echo.
echo The next command should identify the Microsoft C/386 migration thunk:
"%HOST%" --scan "%GUEST%"
if errorlevel 1 goto failed

echo.
echo The next command should run without executing the 16-bit object and print:
echo hello from far16
"%HOST%" --run "%GUEST%"
if errorlevel 1 goto failed

echo.
echo M29B far16 VIO bridge regression complete
exit /b 0

:failed
echo M29B far16 VIO bridge regression FAILED, RC=%ERRORLEVEL%
exit /b %ERRORLEVEL%
