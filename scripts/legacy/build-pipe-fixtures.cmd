@echo off
rem Build the M25/M28D LX pipe fixtures with the recovered C/386 toolchain.
rem Set OS2LIB if LIBC.LIB + OS2386.LIB live somewhere else.

if not "%OS2LIB%"=="" goto have_lib
if exist C:\20ddk\lib\OS2386.LIB set OS2LIB=C:\20ddk\lib
if not "%OS2LIB%"=="" goto have_lib
if exist C:\c386\lib\OS2386.LIB set OS2LIB=C:\c386\lib
if not "%OS2LIB%"=="" goto have_lib

echo Could not find OS2386.LIB.
echo Set OS2LIB to the directory containing LIBC.LIB and OS2386.LIB.
exit /b 1

:have_lib
echo Using OS/2 libraries from %OS2LIB%

cl386 /nologo /c emit-test.c
if errorlevel 1 exit /b 1
cl386 /nologo /c upper-test.c
if errorlevel 1 exit /b 1
cl386 /nologo /c upper-dos-test.c
if errorlevel 1 exit /b 1
cl386 /nologo /c m29m-stderr.c
if errorlevel 1 exit /b 1
cl386 /nologo /c m29m-count.c
if errorlevel 1 exit /b 1

>m28d-emit.lnk echo emit-test.obj
>>m28d-emit.lnk echo emit-test.exe
>>m28d-emit.lnk echo nul.map
>>m28d-emit.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m28d-emit.lnk echo nul.def
link386 @m28d-emit.lnk
if errorlevel 1 exit /b 1

>m28d-upper.lnk echo upper-test.obj
>>m28d-upper.lnk echo upper-test.exe
>>m28d-upper.lnk echo nul.map
>>m28d-upper.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m28d-upper.lnk echo nul.def
link386 @m28d-upper.lnk
if errorlevel 1 exit /b 1

>m28d-upper-dos.lnk echo upper-dos-test.obj
>>m28d-upper-dos.lnk echo upper-dos-test.exe
>>m28d-upper-dos.lnk echo nul.map
>>m28d-upper-dos.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m28d-upper-dos.lnk echo nul.def
link386 @m28d-upper-dos.lnk
if errorlevel 1 exit /b 1



>m29m-stderr.lnk echo m29m-stderr.obj
>>m29m-stderr.lnk echo m29m-stderr.exe
>>m29m-stderr.lnk echo nul.map
>>m29m-stderr.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29m-stderr.lnk echo nul.def
link386 @m29m-stderr.lnk
if errorlevel 1 exit /b 1

>m29m-count.lnk echo m29m-count.obj
>>m29m-count.lnk echo m29m-count.exe
>>m29m-count.lnk echo nul.map
>>m29m-count.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29m-count.lnk echo nul.def
link386 @m29m-count.lnk
if errorlevel 1 exit /b 1
echo.
echo Built emit-test.exe, upper-test.exe, upper-dos-test.exe, m29m-stderr.exe and m29m-count.exe.
