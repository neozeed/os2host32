@echo off
setlocal
pushd "%~dp0"
whp_os2_v2_hi.exe pmhello.exe %* 2>pmhello-trace.txt
if errorlevel 1 goto failed
popd
exit /b 0
:failed
echo FAIL - inspect pmhello-trace.txt
popd
exit /b 1
