@echo off
rem M31G R4 smoke test. R3 resolves through PMWIN.840 then stops at PMWIN.778.
rem R4 exports WinLoadMenu @778 and should advance to the next boundary.
..\..\os2host32.exe --run NEKO.EXE
