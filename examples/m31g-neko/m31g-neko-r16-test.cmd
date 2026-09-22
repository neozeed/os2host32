@echo off
rem M31G R16 - NEKO external resource-DLL relocation smoke test.
if not exist NEKO.DLL (
  echo Put the historical NEKO.DLL in the current directory or OS2LIBPATH first.
)
set OS2_PM_TRACE=1
os2host32.exe --run examples\m31g-neko\NEKO.EXE
