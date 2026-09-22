@echo off
setlocal
if "%C386LIB%"=="" set C386LIB=\c386\lib
cl386 -c -Gs -Od -W3 -B1 C1_386 info1.c
if errorlevel 1 exit /b %errorlevel%
>info1.lnk echo info1.obj
>>info1.lnk echo info1.exe
>>info1.lnk echo info1.map
>>info1.lnk echo %C386LIB%\libc.lib %C386LIB%\os2386.lib
>>info1.lnk echo info1.def
link386 @info1.lnk
if errorlevel 1 exit /b %errorlevel%
cl386 -c -Gs -Od -W3 -B1 C1_386 infotort.c
if errorlevel 1 exit /b %errorlevel%
>infotort.lnk echo infotort.obj
>>infotort.lnk echo infotort.exe
>>infotort.lnk echo infotort.map
>>infotort.lnk echo %C386LIB%\libc.lib %C386LIB%\os2386.lib
>>infotort.lnk echo infotort.def
link386 @infotort.lnk
if errorlevel 1 exit /b %errorlevel%
exit /b 0
