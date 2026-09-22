@echo off
setlocal
set OS2_PM_TRACE=1
set OS2_TRACE_IO=1
set OS2_TRACE_MODULES=1
pushd examples\m31f-jigsaw
..\..\os2host32.exe --run JIGSAW.EXE
set RC=%ERRORLEVEL%
popd
exit /b %RC%
