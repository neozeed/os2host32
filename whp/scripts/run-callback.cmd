@echo off
setlocal
pushd "%~dp0"
whp_os2_v2_hi.exe callback1.exe 2>callback1-trace.txt
if errorlevel 1 goto failed
whp_os2_v2_hi.exe callbackdll.exe 2>callbackdll-trace.txt
if errorlevel 1 goto failed
echo R9 callback suite PASS
popd
exit /b 0
:failed
echo FAIL - inspect callback1-trace.txt or callbackdll-trace.txt
popd
exit /b 1
