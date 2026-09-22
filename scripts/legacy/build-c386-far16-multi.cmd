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

cl386 /c c386-far16-multi-test.c
if errorlevel 1 exit /b 1

rem RUN286/LINK386 has an extremely short command tail.  Keep every prompted
rem field in a response file rather than expanding paths on the command line.
>m29f-far16-multi.lnk echo c386-far16-multi-test.obj
>>m29f-far16-multi.lnk echo c386-far16-multi-test.exe
>>m29f-far16-multi.lnk echo nul.map
>>m29f-far16-multi.lnk echo %C386ROOT%\lib\libc.lib %C386ROOT%\lib\os2386.lib
>>m29f-far16-multi.lnk echo c386-far16-multi-test.def

link386 @m29f-far16-multi.lnk
if errorlevel 1 exit /b 1

echo.
echo Built c386-far16-multi-test.exe
echo Link response file retained as m29f-far16-multi.lnk
echo Scan it with:
echo   os2host32 --scan c386-far16-multi-test.exe
echo Then run it with:
echo   os2host32 --run c386-far16-multi-test.exe
echo It should recognize four C/386 far16 migration thunks.
endlocal
