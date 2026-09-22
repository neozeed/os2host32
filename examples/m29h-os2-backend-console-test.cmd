@echo off
setlocal
set HOST=%~dp0..\os2host32.exe
set GUEST=%~dp0..\cmdos2_os2_console_test.exe

echo === M29H complete real cmdos2_os2 far16 console backend ===
echo.
echo The scan should identify seven backend-owned migration thunks:
echo   VIOCALLS.7  VioScrollUp
echo   VIOCALLS.9  VioGetCurPos
echo   VIOCALLS.15 VioSetCurPos
echo   VIOCALLS.19 VioWrtTTY
echo   VIOCALLS.21 VioGetMode
echo   KBDCALLS.4  KbdCharIn
echo   KBDCALLS.13 KbdFlushBuffer
"%HOST%" --scan "%GUEST%"
if errorlevel 1 goto failed

echo.
echo The run first queries/restores the cursor, clears through VIO mode+scroll,
echo then prints, flushes and reads one key through the real cmdos2_os2 backend.
echo It should finish with M29H_OS2_CONSOLE_BACKEND_OK.
"%HOST%" --run "%GUEST%"
if errorlevel 1 goto failed

echo.
echo M29H complete direct backend console regression complete
goto end

:failed
echo M29H regression FAILED
exit /b 1

:end
endlocal
