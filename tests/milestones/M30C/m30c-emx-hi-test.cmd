@echo off
setlocal

echo === M30C EMX a.out relocation-sidecar test ===
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
  echo ERROR: hi.exe is missing. Copy your known-good emxbind-produced executable here.
  exit /b 2
)
if not exist hi (
  echo ERROR: unbound a.out sidecar "hi" is missing.
  echo        It must be the output from gcc hi.o -o hi BEFORE emxbind hi.
  exit /b 2
)

set OS2LIBPATH=.
set OS2_TRACE_MODULES=1
set OS2_TRACE_EMX_SELF=1
set OS2_TRACE_IO=1
set OS2_EMX_AOUT_SIDECAR=1

echo --- Bound hi.exe scanner ---
os2host32.exe --scan hi.exe
if errorlevel 1 goto :failed

echo.
echo --- EMX DLL scanner ---
os2host32.exe --scan emx.dll
if errorlevel 1 goto :failed

echo.
echo --- Real EMX hi.exe with a.out rebasing ---
os2host32.exe --run hi.exe
set M30C_RC=%ERRORLEVEL%

echo.
echo M30C hi.exe host return code: %M30C_RC%
echo Look for the three "M30C EMX" lines immediately before guest execution.
exit /b %M30C_RC%

:failed
set M30C_RC=%ERRORLEVEL%
echo.
echo M30C prerequisite scan failed with return code: %M30C_RC%
exit /b %M30C_RC%
