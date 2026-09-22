@echo off
setlocal
set HOST=%~dp0..\os2host32.exe
set GUEST=%~dp0..\c386-far16-kbdflush-test.exe

echo === M29E descriptor-driven far16 KbdFlushBuffer regression ===
echo.
echo The scan should identify KBDCALLS.13 KbdFlushBuffer as a C/386 migration thunk:
"%HOST%" --scan "%GUEST%"
if errorlevel 1 goto failed

echo.
echo The run should cross a scalar-only two-byte Pascal frame without entering 16-bit code:
"%HOST%" --run "%GUEST%"
if errorlevel 1 goto failed

echo.
echo M29E descriptor-driven KbdFlushBuffer regression complete
goto end

:failed
echo M29E regression FAILED
exit /b 1

:end
endlocal
