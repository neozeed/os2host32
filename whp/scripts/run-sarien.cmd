@echo off
setlocal
set "WHP_OS2_FIND_LAYOUT=BETA"
rem Sarien leaves DosFindFirst input count uninitialized.
set "WHP_OS2_FIND_ZERO_COUNT=1"
rem Preserve the 320x200 client area expected by this Sarien port.
set "WHP_OS2_SARIEN_GEOMETRY=1"
rem Usage: run-sarien.cmd GAME_DIRECTORY [SARIEN_EXE]
rem The original interpreter discovers the game from its current directory.
if "%~1"=="" goto usage
set "SARIEN_HOST=%~dp0whp_os2_v2_hi.exe"
set "SARIEN_GUEST=%~dp0sarienlx.exe"
if not "%~2"=="" set "SARIEN_GUEST=%~f2"
if not exist "%SARIEN_HOST%" goto missing
if not exist "%SARIEN_GUEST%" goto missing
pushd "%~1"
if errorlevel 1 exit /b 1
"%SARIEN_HOST%" "%SARIEN_GUEST%" 2>"%~dp0sarien-trace.txt"
set "SARIEN_RESULT=%errorlevel%"
popd
if not "%SARIEN_RESULT%"=="0" echo Sarien returned %SARIEN_RESULT% - inspect sarien-trace.txt
exit /b %SARIEN_RESULT%
:missing
echo Need whp_os2_v2_hi.exe and sarienlx.exe beside this script, or supply the guest path.
exit /b 1
:usage
echo Usage: run-sarien.cmd GAME_DIRECTORY [SARIEN_EXE]
echo Example: run-sarien.cmd C:\games\agi-demo C:\sar\sarienlx.exe
exit /b 1
