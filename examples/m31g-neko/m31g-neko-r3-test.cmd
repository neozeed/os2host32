@echo off
rem M31G R3 smoke test. R2 resolves PMSHAPI.129 then stops at PMWIN.837.
rem R3 exports WinQueryWindowPos @837 and should advance to the next boundary.
..\..\os2host32.exe --run NEKO.EXE
