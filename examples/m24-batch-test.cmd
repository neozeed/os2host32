@echo off
echo === M24 cbatch regression ===
echo script=%0 arg1=%1 arg2=%2

status7
if errorlevel 7 echo IF ERRORLEVEL works
if not exist __m24_missing__.tmp echo IF NOT EXIST works
if "same"=="same" echo IF string compare works

for %%i in (one two three) do echo FOR item=%%i

set M24_LOCAL=outer
echo before SETLOCAL: %M24_LOCAL%
setlocal
set M24_LOCAL=inner
echo inside SETLOCAL: %M24_LOCAL%
endlocal
echo after ENDLOCAL: %M24_LOCAL%

call m24-child.cmd alpha beta
call :subroutine red blue

goto done
echo ERROR: GOTO failed

:subroutine
echo subroutine before SHIFT: %0 %1 %2
shift
echo subroutine after SHIFT:  %0 %1 %2
goto :eof

:done
echo GOTO/CALL complete
