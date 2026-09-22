@echo off
setlocal
set HOST=%~dp0..\os2host32.exe
set GUEST=%~dp0..\c386-far16-kbdstring-test.exe
set BUILDER=%~dp0..\build-c386-far16-kbdstring.cmd

echo === M29D C/386 far16 KbdStringIn regression ===
echo.
if not exist "%GUEST%" (
  echo c386-far16-kbdstring-test.exe is not present.
  echo Run "%BUILDER%" from the bundle root first.
  exit /b 1
)

echo The scan should identify KBDCALLS.9 as a C/386 migration thunk:
"%HOST%" --scan "%GUEST%"
if errorlevel 1 goto failed

echo.
echo Type a short line. Try Backspace before pressing Enter.
echo The returned cchIn and text should match the edited line.
"%HOST%" --run "%GUEST%"
if errorlevel 1 goto failed

echo.
echo M29D far16 KbdStringIn regression complete
exit /b 0

:failed
echo M29D far16 KbdStringIn regression FAILED, RC=%ERRORLEVEL%
exit /b %ERRORLEVEL%
