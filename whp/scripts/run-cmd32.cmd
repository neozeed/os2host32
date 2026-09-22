@echo off
setlocal
set "WHP_OS2_FIND_LAYOUT=GA"
set "WHP_OS2_FIND_ZERO_COUNT="
set "WHP_OS2_SARIEN_GEOMETRY="
"%~dp0whp_os2_v2_hi.exe" "%~dp0cmd32os2_os2.exe" %* 2>"%~dp0cmd32-trace.txt"
exit /b %errorlevel%
