@echo off
setlocal
if "%C386LIB%"=="" set C386LIB=\c386\lib
cl386 -c -Gs -Od -W3 -B1 C1_386 gpi1.c
if errorlevel 1 exit /b %errorlevel%
>gpi1.lnk echo gpi1.obj
>>gpi1.lnk echo gpi1.exe
>>gpi1.lnk echo gpi1.map
>>gpi1.lnk echo %C386LIB%\libc.lib %C386LIB%\os2386.lib
>>gpi1.lnk echo gpi1.def
link386 @gpi1.lnk
if errorlevel 1 exit /b %errorlevel%
exit /b 0
