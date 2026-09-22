@echo off
setlocal
echo === M30A1 EMX bridge test ===
if not exist emx.dll echo ERROR: emx.dll missing & exit /b 2
if not exist dhyrstone.exe echo ERROR: dhyrstone.exe missing & exit /b 2
if not exist os2host32.exe echo ERROR: os2host32.exe missing & exit /b 2
echo.
echo --- EMX scanner ---
os2host32.exe --scan emx.dll
if errorlevel 1 exit /b %errorlevel%
echo.
echo --- Dhrystone through EMX ---
set OS2LIBPATH=.;%OS2LIBPATH%
os2host32.exe dhyrstone.exe
set RC=%ERRORLEVEL%
echo.
echo M30A1 Dhrystone return code: %RC%
exit /b %RC%
