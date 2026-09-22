@echo off
setlocal
if "%C386LIB%"=="" set C386LIB=\c386\lib
cl386 -c -Gs -Od -W3 -B1 C1_386 fileio1.c
if errorlevel 1 exit /b %errorlevel%
>fileio1.lnk echo fileio1.obj
>>fileio1.lnk echo fileio1.exe
>>fileio1.lnk echo fileio1.map
>>fileio1.lnk echo %C386LIB%\libc.lib %C386LIB%\os2386.lib
>>fileio1.lnk echo fileio1.def
link386 @fileio1.lnk
exit /b %errorlevel%
