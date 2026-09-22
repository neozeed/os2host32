@echo off
setlocal
pushd "%~dp0"
echo Running sleep1
whp_os2_v2_hi.exe sleep1.exe 2>sleep1-trace.txt
if errorlevel 1 goto failed
echo Running mutex1
whp_os2_v2_hi.exe mutex1.exe 2>mutex1-trace.txt
if errorlevel 1 goto failed
echo Running queue1
whp_os2_v2_hi.exe queue1.exe 2>queue1-trace.txt
if errorlevel 1 goto failed
echo Running recycle1
whp_os2_v2_hi.exe recycle1.exe 2>recycle1-trace.txt
if errorlevel 1 goto failed
echo R8 scheduling and synchronisation suite PASS
popd
exit /b 0
:failed
echo FAIL - inspect the corresponding trace file.
popd
exit /b 1
