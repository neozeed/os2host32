@echo off
setlocal
pushd "%~dp0"
whp_os2_v2_hi.exe info1.exe 2>info1-trace.txt
set result=%errorlevel%
popd
exit /b %result%
