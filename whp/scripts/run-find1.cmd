@echo off
setlocal
cd /d "%~dp0"
if not exist find1.exe exit /b 2
if not exist whp_os2_v2_hi.exe exit /b 2
if exist v2-find-test (
  echo v2-find-test already exists. Inspect the previous test files before removing it.
  exit /b 2
)
mkdir v2-find-test\SUBDIR
if errorlevel 1 exit /b 2
pushd v2-find-test
..\whp_os2_v2_hi.exe ..\find1.exe 2>..\find1-trace.txt
set TESTRC=%errorlevel%
popd
if not "%TESTRC%"=="0" exit /b %TESTRC%
rmdir v2-find-test\SUBDIR
rmdir v2-find-test
exit /b 0
