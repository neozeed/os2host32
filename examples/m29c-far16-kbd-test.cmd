@echo off
setlocal
set HOST=%~dp0..\os2host32.exe
set GUEST=%~dp0..\c386-far16-kbd-test.exe
set BUILDER=%~dp0..\build-c386-far16-kbd.cmd

echo === M29C C/386 far16 KBD bridge regression ===
echo.
if not exist "%GUEST%" (
  echo c386-far16-kbd-test.exe is not present.
  echo Run "%BUILDER%" from the bundle root first.
  exit /b 1
)

echo The scan should identify KBDCALLS.4 as a C/386 migration thunk:
"%HOST%" --scan "%GUEST%"
if errorlevel 1 goto failed

echo.
echo The run should wait for exactly one key through KbdCharIn.
echo After the key is pressed it should print char/scan/status/state/time.
"%HOST%" --run "%GUEST%"
if errorlevel 1 goto failed

echo.
echo M29C far16 KBD bridge regression complete
exit /b 0

:failed
echo M29C far16 KBD bridge regression FAILED, RC=%ERRORLEVEL%
exit /b %ERRORLEVEL%
