@echo off
setlocal
if "%C386ROOT%"=="" set C386ROOT=C:\c386

if not exist "%C386ROOT%\lib\libc.lib" (
  echo Could not find %C386ROOT%\lib\libc.lib
  exit /b 1
)
if not exist "%C386ROOT%\lib\os2386.lib" (
  echo Could not find %C386ROOT%\lib\os2386.lib
  exit /b 1
)

cl386 /c c386-far16-kbd-test.c
if errorlevel 1 exit /b 1

rem LINK386 1.01 is reached through RUN286, so keep its DOS command tail tiny.
rem Feed all five prompted fields through a response file, as in M29B2.
>m29c-far16-kbd.lnk echo c386-far16-kbd-test.obj
>>m29c-far16-kbd.lnk echo c386-far16-kbd-test.exe
>>m29c-far16-kbd.lnk echo nul.map
>>m29c-far16-kbd.lnk echo %C386ROOT%\lib\libc.lib %C386ROOT%\lib\os2386.lib
>>m29c-far16-kbd.lnk echo c386-far16-kbd-test.def

link386 @m29c-far16-kbd.lnk
if errorlevel 1 exit /b 1

echo.
echo Built c386-far16-kbd-test.exe
echo Link response file retained as m29c-far16-kbd.lnk
echo Scan it with:
echo   os2host32 --scan c386-far16-kbd-test.exe
echo Then run it with:
echo   os2host32 --run c386-far16-kbd-test.exe
echo It should wait for one key and report the returned KBDKEYINFO fields.
endlocal
