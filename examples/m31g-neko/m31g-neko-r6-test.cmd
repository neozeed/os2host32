@echo off
rem M31G R6 smoke test. R5 resolves PMWP.203 then stops at PMWIN.780.
rem R6 exports WinLoadPointer @780 and should advance to the next boundary.
..\..\os2host32.exe --run NEKO.EXE
