@echo off
rem M31G R13 - untouched GA NEKO import-complete smoke test.
rem All 63 distinct ordinal imports should resolve; any next failure is runtime behavior.
os2host32.exe --run examples\m31g-neko\NEKO.EXE
