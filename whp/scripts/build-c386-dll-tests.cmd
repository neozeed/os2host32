@echo off
setlocal

rem WHP OS/2 V2 guest-DLL milestone fixtures.
rem Assumes CL386, C1_386 and LINK386 are on PATH.
rem Correct recovered 1991 C/386 pass-1 executable: C1_386 (not C1L_386).

if "%C386LIB%"=="" set C386LIB=\c386\lib

if not exist %C386LIB%\libc.lib (
  echo Cannot find %C386LIB%\libc.lib
  echo Set C386LIB to the directory containing LIBC.LIB and OS2386.LIB.
  exit /b 1
)
if not exist %C386LIB%\os2386.lib (
  echo Cannot find %C386LIB%\os2386.lib
  exit /b 1
)

echo === compiling CRT-free V2DLL.DLL ===
cl386 -c -Gd -Gs -Zl -Od -W3 -B1 C1_386 v2dll.c
if errorlevel 1 exit /b %errorlevel%
>v2dll.lnk echo v2dll.obj
>>v2dll.lnk echo V2DLL.DLL
>>v2dll.lnk echo v2dll.map
>>v2dll.lnk echo.
>>v2dll.lnk echo v2dll.def
link386 @v2dll.lnk
if errorlevel 1 exit /b %errorlevel%

call :buildexe dlltest
if errorlevel 1 exit /b %errorlevel%
call :buildexe dllthread
if errorlevel 1 exit /b %errorlevel%

echo.
echo Built V2DLL.DLL, dlltest.exe and dllthread.exe
echo Keep V2DLL.DLL in the current directory, or set OS2LIBPATH to its directory.
echo.
echo Suggested tests:
echo   whp_os2_v2_hi.exe dlltest.exe
echo   whp_os2_v2_hi.exe dllthread.exe
echo   whp_os2_v2_hi.exe dllthread.exe 30
exit /b 0

:buildexe
set N=%1
echo.
echo === compiling %N%.c ===
cl386 -c -Gs -Od -W3 -B1 C1_386 %N%.c
if errorlevel 1 exit /b %errorlevel%
>%N%.lnk echo %N%.obj
>>%N%.lnk echo %N%.exe
>>%N%.lnk echo %N%.map
>>%N%.lnk echo %C386LIB%\libc.lib %C386LIB%\os2386.lib
>>%N%.lnk echo %N%.def
echo === linking %N%.exe ===
link386 @%N%.lnk
exit /b %errorlevel%
