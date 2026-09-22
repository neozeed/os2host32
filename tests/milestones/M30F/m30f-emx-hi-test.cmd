@echo off
setlocal

echo === M30F EMX signal-exception-focus test ===
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
  echo ERROR: emx.dll is missing. Copy the EMX runtime DLL here.
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
set EMXOPT=

echo --- Bound hi.exe scanner ---
os2host32.exe --scan hi.exe > m30f-hi-scan.tmp
if errorlevel 1 (
  type m30f-hi-scan.tmp
  del m30f-hi-scan.tmp >nul 2>nul
  goto :failed
)
type m30f-hi-scan.tmp
findstr /I /C:"emx.1" m30f-hi-scan.tmp >nul
if errorlevel 1 (
  echo.
  echo ERROR: hi.exe is not the EMX-bound probe ^(emx.1 import not found^).
  del m30f-hi-scan.tmp >nul 2>nul
  exit /b 2
)
del m30f-hi-scan.tmp >nul 2>nul

echo.
echo --- EMX DLL scanner ---
os2host32.exe --scan emx.dll
if errorlevel 1 goto :failed

echo.
echo --- Real EMX hi.exe: a.out rebase + TIB + exception chain + signal focus ---
os2host32.exe --run hi.exe
set M30F_RC=%ERRORLEVEL%

echo.
echo M30F hi.exe host return code: %M30F_RC%
echo Look for "M30F SIGNAL:" after the M30E EXCEPT line.
exit /b %M30F_RC%

:failed
set M30F_RC=%ERRORLEVEL%
echo.
echo M30F prerequisite scan failed with return code: %M30F_RC%
exit /b %M30F_RC%
