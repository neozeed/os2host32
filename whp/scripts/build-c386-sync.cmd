@echo off
setlocal
if "%C386LIB%"=="" set C386LIB=\c386\lib
cl386 -c -Gs -Od -W3 -B1 C1_386 sleep1.c
if errorlevel 1 exit /b %errorlevel%
>sleep1.lnk echo sleep1.obj
>>sleep1.lnk echo sleep1.exe
>>sleep1.lnk echo sleep1.map
>>sleep1.lnk echo %C386LIB%\libc.lib %C386LIB%\os2386.lib
>>sleep1.lnk echo sleep1.def
link386 @sleep1.lnk
if errorlevel 1 exit /b %errorlevel%
cl386 -c -Gs -Od -W3 -B1 C1_386 mutex1.c
if errorlevel 1 exit /b %errorlevel%
>mutex1.lnk echo mutex1.obj
>>mutex1.lnk echo mutex1.exe
>>mutex1.lnk echo mutex1.map
>>mutex1.lnk echo %C386LIB%\libc.lib %C386LIB%\os2386.lib
>>mutex1.lnk echo mutex1.def
link386 @mutex1.lnk
if errorlevel 1 exit /b %errorlevel%
cl386 -c -Gs -Od -W3 -B1 C1_386 queue1.c
if errorlevel 1 exit /b %errorlevel%
>queue1.lnk echo queue1.obj
>>queue1.lnk echo queue1.exe
>>queue1.lnk echo queue1.map
>>queue1.lnk echo %C386LIB%\libc.lib %C386LIB%\os2386.lib
>>queue1.lnk echo queue1.def
link386 @queue1.lnk
if errorlevel 1 exit /b %errorlevel%
cl386 -c -Gs -Od -W3 -B1 C1_386 recycle1.c
if errorlevel 1 exit /b %errorlevel%
>recycle1.lnk echo recycle1.obj
>>recycle1.lnk echo recycle1.exe
>>recycle1.lnk echo recycle1.map
>>recycle1.lnk echo %C386LIB%\libc.lib %C386LIB%\os2386.lib
>>recycle1.lnk echo recycle1.def
link386 @recycle1.lnk
if errorlevel 1 exit /b %errorlevel%
exit /b 0
