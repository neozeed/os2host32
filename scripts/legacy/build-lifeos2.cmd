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

copy /y tests\vio\lifeos2.c . >nul
copy /y tests\vio\lifeos2.def . >nul

rem The supplied source defines INCL_16 before os2.h.  The Microsoft C/386
rem headers therefore emit the historical 32->16 migration calls that
rem OS2HOST32 recognizes as metadata and replaces with native bridges.
cl386 /c lifeos2.c
if errorlevel 1 exit /b 1

>vio-life.lnk echo lifeos2.obj
>>vio-life.lnk echo lifeos2.exe
>>vio-life.lnk echo lifeos2.map
>>vio-life.lnk echo %C386ROOT%\lib\libc.lib %C386ROOT%\lib\os2386.lib
>>vio-life.lnk echo lifeos2.def

link386 @vio-life.lnk
if errorlevel 1 exit /b 1

echo Built lifeos2.exe
echo   os2host32 --scan lifeos2.exe
echo   os2host32 --run  lifeos2.exe
echo   os2host32 --run  lifeos2.exe C
endlocal
