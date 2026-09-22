@echo off
rem M31G R8 - WinDestroyPointer boundary smoke test.
if not exist PMWIN.dll (
  echo Build PMWIN.dll first.
  exit /b 1
)
os2host32.exe --run examples\m31g-neko\NEKO.EXE
