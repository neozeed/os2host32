@echo off
setlocal

echo === M31A WMCHAR LINK386 fixup regression ===
python tools\check_m31a_wmchar_fixups.py examples\m31a-wmchar\WMCHAR.EXE
if errorlevel 1 goto :fail

echo.
echo === M31A WMCHAR PM stack regression ===
python tools\check_m31a_wmchar_stack.py examples\m31a-wmchar\WMCHAR.EXE
if errorlevel 1 goto :fail

echo.
echo === M31A WMCHAR import regression ===
python tools\check_m31a_wmchar_imports.py os2host32.exe examples\m31a-wmchar\WMCHAR.EXE
if errorlevel 1 goto :fail

echo.
echo === M31A WMCHAR import scan ===
os2host32 --scan examples\m31a-wmchar\WMCHAR.EXE
if errorlevel 1 goto :fail

echo.
echo === M31A WMCHAR run ===
echo Expected: Char Messages window; type letters, arrows and F-keys.
echo Close the native window to end the test.
os2host32 --run examples\m31a-wmchar\WMCHAR.EXE
if errorlevel 1 goto :fail

echo.
echo M31A WMCHAR completed.
exit /b 0

:fail
echo.
echo M31A WMCHAR FAILED with errorlevel %errorlevel%.
exit /b %errorlevel%
