@echo off
rem M31G R10 - PrfOpenProfile boundary smoke test.
if not exist PMSHAPI.dll (
  echo Build PMSHAPI.dll first.
  exit /b 1
)
os2host32.exe --run examples\m31g-neko\NEKO.EXE
