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

cl386 /c c386-far16-vio-test.c
if errorlevel 1 exit /b 1

rem LINK386 1.01 is a 16-bit program reached through RUN286.  Keep the DOS
rem command tail tiny: long absolute library paths can otherwise truncate the
rem final DEF-file argument.  Feed the historical linker its five prompted
rem fields through a response file instead.
>m29b-far16-vio.lnk echo c386-far16-vio-test.obj
>>m29b-far16-vio.lnk echo c386-far16-vio-test.exe
>>m29b-far16-vio.lnk echo nul.map
>>m29b-far16-vio.lnk echo %C386ROOT%\lib\libc.lib %C386ROOT%\lib\os2386.lib
>>m29b-far16-vio.lnk echo c386-far16-vio-test.def

link386 @m29b-far16-vio.lnk
if errorlevel 1 exit /b 1

echo.
echo Built c386-far16-vio-test.exe
echo Link response file retained as m29b-far16-vio.lnk
echo Scan it with:
echo   os2host32 --scan c386-far16-vio-test.exe
echo Then run it with:
echo   os2host32 --run c386-far16-vio-test.exe
endlocal
