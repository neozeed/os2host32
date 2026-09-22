@echo off
set OS2_PM_TRACE=1
set OS2_PM_BYPASS_WM_CREATE=1
os2host32.exe --run WMCHAR.EXE
