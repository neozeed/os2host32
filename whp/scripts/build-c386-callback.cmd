@echo off
setlocal
if "%C386LIB%"=="" set C386LIB=\c386\lib
cl386 -c -Gd -Gs -Zl -Od -W3 -B1 C1_386 cbdll.c
if errorlevel 1 exit /b %errorlevel%
>cbdll.lnk echo cbdll.obj
>>cbdll.lnk echo cbdll.dll
>>cbdll.lnk echo cbdll.map
>>cbdll.lnk echo.
>>cbdll.lnk echo cbdll.def
link386 @cbdll.lnk
if errorlevel 1 exit /b %errorlevel%
cl386 -c -Gs -Od -W3 -B1 C1_386 callback1.c
if errorlevel 1 exit /b %errorlevel%
>callback1.lnk echo callback1.obj
>>callback1.lnk echo callback1.exe
>>callback1.lnk echo callback1.map
>>callback1.lnk echo %C386LIB%\libc.lib %C386LIB%\os2386.lib
>>callback1.lnk echo callback1.def
link386 @callback1.lnk
if errorlevel 1 exit /b %errorlevel%
cl386 -c -Gs -Od -W3 -B1 C1_386 callbackdll.c
if errorlevel 1 exit /b %errorlevel%
>callbackdll.lnk echo callbackdll.obj
>>callbackdll.lnk echo callbackdll.exe
>>callbackdll.lnk echo callbackdll.map
>>callbackdll.lnk echo %C386LIB%\libc.lib %C386LIB%\os2386.lib
>>callbackdll.lnk echo callbackdll.def
link386 @callbackdll.lnk
if errorlevel 1 exit /b %errorlevel%
exit /b 0
