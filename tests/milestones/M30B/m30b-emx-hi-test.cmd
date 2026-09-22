@echo off
setlocal

echo === M30B EMX bound-executable self-inspection trace ===
echo.

if not exist os2host32.exe (
  echo ERROR: os2host32.exe is missing. Build the tree first.
  exit /b 2
)
if not exist DOSCALLS.dll (
  echo ERROR: DOSCALLS.dll is missing. Build the compatibility DLLs first.
  exit /b 2
)
if not exist emx.dll (
  echo ERROR: emx.dll is missing. Copy the supplied EMX 0.9d DLL here.
  exit /b 2
)
if not exist hi.exe (
  echo ERROR: hi.exe is missing. Copy your emxbind-produced test executable here.
  exit /b 2
)

set OS2LIBPATH=.
set OS2_TRACE_MODULES=1
set OS2_TRACE_EMX_SELF=1
set OS2_TRACE_IO=1

echo --- EMX bridge scanner ---
os2host32.exe --scan emx.dll
if errorlevel 1 goto :done

echo.
echo --- Real hi.exe execution with M30B self-inspection trace ---
os2host32.exe --run hi.exe
set M30B_RC=%ERRORLEVEL%

echo.
echo M30B hi.exe return code: %M30B_RC%

echo.
echo Please capture everything from GUESTMOD INITCALL through the final return.
exit /b %M30B_RC%

:done
set M30B_RC=%ERRORLEVEL%
echo.
echo M30B scanner failed with return code: %M30B_RC%
exit /b %M30B_RC%
