@echo off
echo child before SHIFT: %0 %1 %2
shift
echo child after SHIFT:  %0 %1 %2
goto :eof
