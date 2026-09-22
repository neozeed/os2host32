@echo off
setlocal
if "%C386LIB%"=="" set C386LIB=\c386\lib
cl386 -c -Gs -Od -W3 -B1 C1_386 find1.c
if errorlevel 1 exit /b %errorlevel%
>find1.lnk echo find1.obj
>>find1.lnk echo find1.exe
>>find1.lnk echo find1.map
>>find1.lnk echo %C386LIB%\libc.lib %C386LIB%\os2386.lib
>>find1.lnk echo find1.def
link386 @find1.lnk
exit /b %errorlevel%
