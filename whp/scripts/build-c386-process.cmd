@echo off
setlocal
if "%C386LIB%"=="" set C386LIB=\c386\lib
call :build execchild
if errorlevel 1 exit /b %errorlevel%
call :build process1
exit /b %errorlevel%
:build
cl386 -c -Gs -Od -W3 -B1 C1_386 %1.c
if errorlevel 1 exit /b %errorlevel%
>%1.lnk echo %1.obj
>>%1.lnk echo %1.exe
>>%1.lnk echo %1.map
>>%1.lnk echo %C386LIB%\libc.lib %C386LIB%\os2386.lib
>>%1.lnk echo %1.def
link386 @%1.lnk
exit /b %errorlevel%
