@echo off
setlocal
pushd "%~dp0"
whp_os2_v2_hi.exe gpi1.exe %* 2>gpi1-trace.txt
if errorlevel 1 goto failed
popd
exit /b 0
:failed
echo FAIL - inspect gpi1-trace.txt
popd
exit /b 1
