@echo off
setlocal
rem Usage: run-infotort [runs=10] [workers=10, max30] [rounds=100]
set runs=10
set workers=10
set rounds=100
if not "%~1"=="" set runs=%~1
if not "%~2"=="" set workers=%~2
if not "%~3"=="" set rounds=%~3
rem Validate run count before FOR /L; guest strictly validates other counts.
for /f "delims=0123456789" %%A in ("%runs%") do exit /b 2
if %runs% LSS 1 exit /b 2
if %runs% GTR 1000000 exit /b 2
pushd "%~dp0"
for /L %%R in (1,1,%runs%) do (
    echo infotort run %%R/%runs%
    whp_os2_v2_hi.exe infotort.exe %workers% %rounds% 2>infotort-trace.txt
    if errorlevel 1 goto failed
)
popd
exit /b 0
:failed
echo FAIL - inspect infotort-trace.txt for the failed run.
popd
exit /b 1
