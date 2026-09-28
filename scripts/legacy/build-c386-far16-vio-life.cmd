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

copy /y tests\legacy\c386-far16-vio-life-test.c . >nul
copy /y tests\legacy\c386-far16-vio-life-test.def . >nul

cl386 /c c386-far16-vio-life-test.c
if errorlevel 1 exit /b 1

>vio-life-far16.lnk echo c386-far16-vio-life-test.obj
>>vio-life-far16.lnk echo c386-far16-vio-life-test.exe
>>vio-life-far16.lnk echo nul.map
>>vio-life-far16.lnk echo %C386ROOT%\lib\libc.lib %C386ROOT%\lib\os2386.lib
>>vio-life-far16.lnk echo c386-far16-vio-life-test.def

link386 @vio-life-far16.lnk
if errorlevel 1 exit /b 1

echo Built c386-far16-vio-life-test.exe
echo   os2host32 --scan c386-far16-vio-life-test.exe
echo   os2host32 --run  c386-far16-vio-life-test.exe
endlocal
