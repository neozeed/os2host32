@echo off
setlocal
set OS2_PM_TRACE=1
set OS2_TRACE_IO=1
set OS2_TRACE_MODULES=1
pushd examples\m31f-jigsaw
..\..\os2host32.exe --run JIGSAW.EXE 1>..\..\jigsaw-r5-out.txt 2>..\..\jigsaw-r5-err.txt
set RC=%ERRORLEVEL%
popd
echo stdout: jigsaw-r5-out.txt
echo stderr: jigsaw-r5-err.txt
exit /b %RC%
