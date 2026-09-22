@echo off
setlocal
echo === M30A2 EMX bridge execution test ===
if not exist emx.dll echo ERROR: emx.dll missing & exit /b 2
if not exist dhyrstone.exe echo ERROR: dhyrstone.exe missing & exit /b 2
if not exist os2host32.exe echo ERROR: os2host32.exe missing & exit /b 2

echo.
echo --- EMX scanner ---
os2host32.exe --scan emx.dll
if errorlevel 1 exit /b %errorlevel%

echo.
echo --- Dhrystone through EMX: ACTUAL EXECUTION ---
set OS2LIBPATH=.;%OS2LIBPATH%
set OS2_TRACE_MODULES=1
os2host32.exe --run dhyrstone.exe
set RC=%ERRORLEVEL%

echo.
echo M30A2 Dhrystone return code: %RC%
exit /b %RC%
