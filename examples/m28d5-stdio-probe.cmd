@echo off
rem Run this from ordinary Windows CMD, not from cmd32os2.
rem OS2_TRACE_IO traces DOSCALLS.224/.256/.281/.282 to stderr.

del m28d5-source.txt 2>nul
del m28d5-api-upper.txt 2>nul
del m28d5-stdio-upper.txt 2>nul

..\os2host32.exe --run-quiet ..\emit-test.exe > m28d5-source.txt
echo emit RC=%ERRORLEVEL%

echo --- direct DosRead/DosWrite filter ---
..\os2host32.exe --run-quiet ..\upper-dos-test.exe < m28d5-source.txt > m28d5-api-upper.txt
echo API filter RC=%ERRORLEVEL%
type m28d5-api-upper.txt

echo --- recovered C/386 stdio filter with DOSCALLS trace ---
set OS2_TRACE_IO=1
..\os2host32.exe --run-quiet ..\upper-test.exe < m28d5-source.txt > m28d5-stdio-upper.txt
echo stdio filter RC=%ERRORLEVEL%
set OS2_TRACE_IO=
type m28d5-stdio-upper.txt
