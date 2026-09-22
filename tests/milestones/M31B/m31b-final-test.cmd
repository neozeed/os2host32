@echo off
rem M31B FINAL - frozen WMCHAR resource/menu/icon integration test
python tools\check_m31b_wmchar_resources.py examples\m31a-wmchar\WMCHAR.EXE examples\m31a-wmchar\WMCHAR.ICO
if errorlevel 1 exit /b 1
python tools\check_m31a_wmchar_fixups.py examples\m31a-wmchar\WMCHAR.EXE
if errorlevel 1 exit /b 1
python tools\check_m31a_wmchar_stack.py examples\m31a-wmchar\WMCHAR.EXE
if errorlevel 1 exit /b 1
os2host32.exe --scan examples\m31a-wmchar\WMCHAR.EXE
if errorlevel 1 exit /b 1
set OS2_PM_TRACE=1
os2host32.exe --run examples\m31a-wmchar\WMCHAR.EXE
