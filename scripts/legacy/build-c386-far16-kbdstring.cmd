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

cl386 /c c386-far16-kbdstring-test.c
if errorlevel 1 exit /b 1

rem LINK386 1.01 is reached through RUN286, so keep its DOS command tail tiny.
rem As in M29B2/M29C, all five prompted fields live in a response file.
>m29d-far16-kbdstring.lnk echo c386-far16-kbdstring-test.obj
>>m29d-far16-kbdstring.lnk echo c386-far16-kbdstring-test.exe
>>m29d-far16-kbdstring.lnk echo nul.map
>>m29d-far16-kbdstring.lnk echo %C386ROOT%\lib\libc.lib %C386ROOT%\lib\os2386.lib
>>m29d-far16-kbdstring.lnk echo c386-far16-kbdstring-test.def

link386 @m29d-far16-kbdstring.lnk
if errorlevel 1 exit /b 1

echo.
echo Built c386-far16-kbdstring-test.exe
echo Link response file retained as m29d-far16-kbdstring.lnk
echo Scan it with:
echo   os2host32 --scan c386-far16-kbdstring-test.exe
echo Then run it with:
echo   os2host32 --run c386-far16-kbdstring-test.exe
echo Type a short line and press Enter. Backspace should edit the line.
endlocal
