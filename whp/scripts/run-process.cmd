@echo off
setlocal
pushd "%~dp0"
whp_os2_v2_hi.exe process1.exe 2>process-trace.txt
set result=%errorlevel%
if not "%result%"=="0" echo FAIL - inspect process-trace.txt
popd
exit /b %result%
