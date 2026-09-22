@echo off
setlocal

rem R3 copies the proven R2 tests.  The only intended build change is the
rem recovered compiler pass name: C1_386, not C1L_386.
if "%C386LIB%"=="" set C386LIB=\c386\lib

call :build threadtort
if errorlevel 1 exit /b %errorlevel%
call :build sem1
if errorlevel 1 exit /b %errorlevel%
call :build semtort
if errorlevel 1 exit /b %errorlevel%
exit /b 0

:build
set N=%1
cl386 -c -Gs -Od -W3 -B1 C1_386 %N%.c
if errorlevel 1 exit /b %errorlevel%
>%N%.lnk echo %N%.obj
>>%N%.lnk echo %N%.exe
>>%N%.lnk echo %N%.map
>>%N%.lnk echo %C386LIB%\libc.lib %C386LIB%\os2386.lib
>>%N%.lnk echo %N%.def
link386 @%N%.lnk
exit /b %errorlevel%
