@echo off
set OS2LIBPATH=.
set OS2_TRACE_MODULES=1
set OS2_TRACE_EMX_SELF=1
set OS2_TRACE_IO=1
set OS2_TRACE_NLS=1
set OS2_EMX_AOUT_SIDECAR=1

echo === M30I EMX DosSub* heap test ===
echo.
if not exist hi.exe (
  echo ERROR: hi.exe is missing.
  exit /b 2
)
if not exist hi (
  echo ERROR: unbound a.out sidecar "hi" is missing.
  exit /b 2
)
if not exist emx.dll (
  echo ERROR: emx.dll is missing.
  exit /b 2
)
if not exist NLS.dll (
  echo ERROR: NLS.dll is missing - run make compat first.
  exit /b 2
)

echo --- Bound hi.exe scanner ---
os2host32.exe --scan hi.exe | findstr /i "emx"
if errorlevel 1 (
  echo ERROR: hi.exe does not appear to import EMX. Do not use the native LX hello here.
  exit /b 3
)

echo.
echo --- Real EMX hi.exe with OS/2 DosSub* pool support ---
os2host32.exe --run hi.exe
set RC=%ERRORLEVEL%
echo.
echo M30I hi.exe host return code: %RC%
exit /b %RC%
