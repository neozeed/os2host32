@echo off
setlocal
if "%C386LIB%"=="" set C386LIB=\c386\lib
cl386 -c -Gs -Od -W3 -B1 C1_386 sarprep1.c
if errorlevel 1 exit /b %errorlevel%
>sarprep1.lnk echo sarprep1.obj
>>sarprep1.lnk echo sarprep1.exe
>>sarprep1.lnk echo sarprep1.map
>>sarprep1.lnk echo %C386LIB%\libc.lib %C386LIB%\os2386.lib
>>sarprep1.lnk echo sarprep1.def
link386 @sarprep1.lnk
if errorlevel 1 exit /b %errorlevel%
exit /b 0
