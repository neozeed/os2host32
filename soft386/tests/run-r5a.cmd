@echo off
setlocal
pushd "%~dp0..\.."
soft386\soft386_os2.exe --run-quiet soft386\tests\fixtures\resource-load-soft386.le
if errorlevel 1 goto failed
call soft386\tests\run-r5.cmd
if errorlevel 1 goto failed
echo Soft386 R5A Windows smoke PASS
popd
exit /b 0
:failed
set "S386_RC=%ERRORLEVEL%"
echo Soft386 R5A FAILED, errorlevel %S386_RC%.
popd
exit /b %S386_RC%
