@echo off
setlocal
set "WHP_OS2_FIND_LAYOUT=GA"
"%~dp0whp_os2_v2_hi.exe" "%~dp0cmd32-source\cmdos2_os2_console_test.exe" 2>"%~dp0console-trace.txt"
exit /b %errorlevel%
