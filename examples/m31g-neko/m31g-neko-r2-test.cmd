@echo off
rem M31G R2 smoke test. R1 maps the LX image then stops at PMSHAPI.129.
rem R2 exports WinRemoveSwitchEntry @129 and should advance to the next boundary.
..\..\os2host32.exe --run NEKO.EXE
