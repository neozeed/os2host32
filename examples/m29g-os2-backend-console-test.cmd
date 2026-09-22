@echo off
setlocal
set HOST=%~dp0..\os2host32.exe
set GUEST=%~dp0..\cmdos2_os2_console_test.exe

echo === M29G real cmdos2_os2 far16 console backend ===
echo.
echo The scan should identify exactly three backend-owned migration thunks:
echo   VIOCALLS.19 VioWrtTTY
echo   KBDCALLS.4  KbdCharIn
echo   KBDCALLS.13 KbdFlushBuffer
"%HOST%" --scan "%GUEST%"
if errorlevel 1 goto failed

echo.
echo The run should print through CmdO2VioWrtTTY, flush through
echo CmdO2KbdFlushBuffer, wait for one key through CmdO2KbdCharIn,
echo and finish with M29G_OS2_CONSOLE_BACKEND_OK.
"%HOST%" --run "%GUEST%"
if errorlevel 1 goto failed

echo.
echo M29G direct backend console regression complete
goto end

:failed
echo M29G regression FAILED
exit /b 1

:end
endlocal
