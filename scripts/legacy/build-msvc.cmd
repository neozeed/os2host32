@echo off
rem Builds both host tools and the complete compatibility DLL set.
rem Microsoft C/C++ 8.x / Visual C++ 1.10 oriented build sketch.
rem le2pe386.c remains ANSI C89.
cl /O2 /W3 le2pe386.c /link /out:le2pe386.exe
cl /O2 /W3 os2host32.c /link /out:os2host32.exe
cl /O2 /W3 cmd32os2.c cmdparse.c cmdbatch.c cmdfile.c cmdos2_win32.c cmdos2_env.c /link /out:cmd32os2.exe kernel32.lib

cl /O2 /W3 /LD doscalls.c /link /def:doscalls.def /out:DOSCALLS.dll kernel32.lib winmm.lib
cl /O2 /W3 /LD kbdcalls.c /link /def:kbdcalls.def /out:KBDCALLS.dll kernel32.lib
cl /O2 /W3 /LD viocalls.c /link /def:viocalls.def /out:VIOCALLS.dll kernel32.lib
cl /O2 /W3 /LD quecalls.c /link /def:quecalls.def /out:QUECALLS.dll kernel32.lib
cl /O2 /W3 /LD pmwin.c   /link /def:pmwin.def   /out:PMWIN.dll kernel32.lib user32.lib gdi32.lib
cl /O2 /W3 /LD pmgpi.c   /link /def:pmgpi.def   /out:PMGPI.dll kernel32.lib gdi32.lib
