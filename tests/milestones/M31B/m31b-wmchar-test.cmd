@echo off
rem M31B R1 - original Microsoft Beta-2 WMCHAR resource/menu/icon test
python tools\check_m31b_wmchar_resources.py examples\m31a-wmchar\WMCHAR.EXE examples\m31a-wmchar\WMCHAR.ICO
if errorlevel 1 exit /b 1
os2host32.exe --scan examples\m31a-wmchar\WMCHAR.EXE
if errorlevel 1 exit /b 1
set OS2_PM_TRACE=1
os2host32.exe --run examples\m31a-wmchar\WMCHAR.EXE
