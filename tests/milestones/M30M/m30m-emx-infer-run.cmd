@echo off
set OS2LIBPATH=.
set OS2_EMX_AOUT_SIDECAR=0
set OS2_EMX_INFER_RELOCS=1
set OS2_EMX_FIXED_DATA=1
set OS2_TRACE_MODULES=1
set OS2_TRACE_EMX_SELF=1
set OS2_TRACE_IO=1
set OS2_TRACE_NLS=1

echo === M30M4 sidecarless EMX relocation diagnostic run ===
echo Command: %*
echo.
echo WARNING: the retained hi oracle still shows two false-positive TEXT candidates.
echo This run applies the inferred set and records nearby patch sites if the guest faults.
echo.
if "%~1"=="" (
  echo ERROR: usage: m30m-emx-infer-run.cmd program.exe [guest arguments ...]
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

os2host32.exe --run %*
set RC=%ERRORLEVEL%
echo.
echo M30M experimental host return code: %RC%
echo Look for "M30M EMX infer   : mode=patch" and program output.
exit /b %RC%
