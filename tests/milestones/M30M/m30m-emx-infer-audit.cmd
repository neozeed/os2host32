@echo off
set OS2LIBPATH=.
set OS2_EMX_AOUT_SIDECAR=0
set OS2_EMX_INFER_RELOCS=audit
set OS2_EMX_FIXED_DATA=1
set OS2_TRACE_MODULES=0

echo === M30M bound-EXE EMX relocation recovery AUDIT ===
echo Target: %1
echo.
echo No a.out sidecar is used.  This pass does NOT patch inferred sites and does NOT execute the guest.
echo.
if "%~1"=="" (
  echo ERROR: usage: m30m-emx-infer-audit.cmd program.exe
  exit /b 2
)
if not exist "%~1" (
  echo ERROR: %~1 is missing.
  exit /b 2
)
if not exist os2host32.exe (
  echo ERROR: os2host32.exe is missing - run make tools first.
  exit /b 2
)
if not exist emx.dll (
  echo ERROR: emx.dll is missing.
  exit /b 2
)

os2host32.exe --fixups "%~1"
set RC=%ERRORLEVEL%
echo.
echo M30M audit host return code: %RC%
echo Look for "M30M EMX infer   : mode=audit" and "AUDIT ONLY".
exit /b %RC%
