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

cl386 /c c386-far16-kbdflush-test.c
if errorlevel 1 exit /b 1

rem Keep the RUN286/LINK386 command tail short: feed the five prompted fields
rem through a response file, exactly as in M29B2-M29D.
>m29e-far16-kbdflush.lnk echo c386-far16-kbdflush-test.obj
>>m29e-far16-kbdflush.lnk echo c386-far16-kbdflush-test.exe
>>m29e-far16-kbdflush.lnk echo nul.map
>>m29e-far16-kbdflush.lnk echo %C386ROOT%\lib\libc.lib %C386ROOT%\lib\os2386.lib
>>m29e-far16-kbdflush.lnk echo c386-far16-kbdflush-test.def

link386 @m29e-far16-kbdflush.lnk
if errorlevel 1 exit /b 1

echo.
echo Built c386-far16-kbdflush-test.exe
echo Link response file retained as m29e-far16-kbdflush.lnk
echo Scan it with:
echo   os2host32 --scan c386-far16-kbdflush-test.exe
echo Then run it with:
echo   os2host32 --run c386-far16-kbdflush-test.exe
echo Expected result: KbdFlushBuffer rc=0
endlocal
