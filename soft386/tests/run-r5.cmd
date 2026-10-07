@echo off
setlocal
pushd "%~dp0..\.."
set "S386=soft386\soft386_os2.exe"
set "FX=soft386\tests\fixtures"
"%S386%" --run-quiet "%FX%\dll-static-soft386.le"
if errorlevel 1 goto failed
"%S386%" --run-quiet "%FX%\dll-static-soft386.lx"
if errorlevel 1 goto failed
"%S386%" --run-quiet "%FX%\dll-api-soft386.le"
if errorlevel 1 goto failed
"%S386%" --run-quiet "%FX%\dll-legacy-soft386.le"
if errorlevel 1 goto failed
"%S386%" --max-cycles 0 --trace-hc --trace-native "%FX%\pmjar-soft386.exe" 2>soft386-r5-pm.trace
if errorlevel 1 goto failed
 echo Soft386 R5 Windows smoke PASS
popd
exit /b 0
:failed
set "S386_RC=%ERRORLEVEL%"
echo Soft386 R5 FAILED, errorlevel %S386_RC%. Retain soft386-r5-pm.trace if present.
popd
exit /b %S386_RC%
