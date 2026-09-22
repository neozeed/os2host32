@echo off
setlocal
set OS2_PM_TRACE=1
set OS2_TRACE_IO=1
set OS2_TRACE_MODULES=1
pushd examples\m31f-jigsaw
..\..\os2host32.exe --run JIGSAW.EXE 1>..\..\jigsaw-r6-out.txt 2>..\..\jigsaw-r6-err.txt
set RC=%ERRORLEVEL%
popd
echo stdout: jigsaw-r6-out.txt
echo stderr: jigsaw-r6-err.txt
exit /b %RC%
