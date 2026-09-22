@echo off
setlocal
set HOST=%~dp0..\os2host32.exe
set GUEST=%~dp0..\c386-far16-multi-test.exe

echo === M29F multiple C/386 far16 thunk regression ===
echo.
echo The scan should identify four migration thunks in one mixed 16/32-bit LE:
"%HOST%" --scan "%GUEST%"
if errorlevel 1 goto failed

echo.
echo The run should flush input, read one key, then read an edited line.
"%HOST%" --run "%GUEST%"
if errorlevel 1 goto failed

echo.
echo M29F multi-thunk regression complete
goto end

:failed
echo M29F regression FAILED
exit /b 1

:end
endlocal
