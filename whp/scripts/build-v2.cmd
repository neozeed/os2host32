@echo off
setlocal
cl /nologo /W4 /O2 /D_CRT_SECURE_NO_WARNINGS whp_os2_v2_hi.c /link WinHvPlatform.lib User32.lib Gdi32.lib
