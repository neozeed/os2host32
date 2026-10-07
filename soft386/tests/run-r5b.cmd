@echo off
setlocal
pushd "%~dp0..\.."
soft386\soft386_os2.exe --run-quiet --trace-native soft386\tests\fixtures\queue-soft386.le 2>soft386-r5b-queue.trace
if errorlevel 1 goto failed
call soft386\tests\run-r5a.cmd
if errorlevel 1 goto failed
echo Soft386 R5B Windows smoke PASS
popd
exit /b 0
:failed
set "S386_RC=%ERRORLEVEL%"
echo Soft386 R5B FAILED, errorlevel %S386_RC%. Keep soft386-r5b-queue.trace.
popd
exit /b %S386_RC%
