@echo off
rem M31G R5 smoke test. IMPORTANT: PMWP.dll in the tree must be the native
rem compatibility DLL built from pmwp.c, not the historical IBM GA PMWP.DLL.
..\..\os2host32.exe --run NEKO.EXE
