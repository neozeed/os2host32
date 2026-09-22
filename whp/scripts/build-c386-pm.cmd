@echo off
setlocal
if "%C386LIB%"=="" set C386LIB=\c386\lib
cl386 -c -Gs -Od -W3 -B1 C1_386 pmhello.c
if errorlevel 1 exit /b %errorlevel%
>pmhello.lnk echo pmhello.obj
>>pmhello.lnk echo pmhello.exe
>>pmhello.lnk echo pmhello.map
>>pmhello.lnk echo %C386LIB%\libc.lib %C386LIB%\os2386.lib
>>pmhello.lnk echo pmhello.def
link386 @pmhello.lnk
if errorlevel 1 exit /b %errorlevel%
exit /b 0
