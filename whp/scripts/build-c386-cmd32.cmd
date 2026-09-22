@echo off
setlocal
if "%OS2LIB%"=="" set "OS2LIB=C:\c386\lib"
pushd "%~dp0cmd32-source"
if errorlevel 1 exit /b 1
for %%N in (cmdparse cmdbatch cmdfile cmd32os2 cmdos2_os2 cmdos2_session_os2 cmdos2_env cmdos2_os2_console_test) do (
  cl386 /nologo /Gd /c %%N.c
  if errorlevel 1 goto fail
)
>cmd32.lnk echo cmd32os2.obj cmdparse.obj cmdbatch.obj cmdfile.obj cmdos2_os2.obj cmdos2_session_os2.obj cmdos2_env.obj
>>cmd32.lnk echo cmd32os2_os2.exe
>>cmd32.lnk echo cmd32os2_os2.map
>>cmd32.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>cmd32.lnk echo cmd32os2_os2.def
link386 @cmd32.lnk
if errorlevel 1 goto fail
>console.lnk echo cmdos2_os2_console_test.obj cmdos2_os2.obj cmdos2_env.obj
>>console.lnk echo cmdos2_os2_console_test.exe
>>console.lnk echo cmdos2_os2_console_test.map
>>console.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>console.lnk echo cmdos2_os2_console_test.def
link386 @console.lnk
if errorlevel 1 goto fail
popd
echo Built CMD32 and console test under cmd32-source.
echo The supplied cmd32os2_os2.exe beside the host has not been replaced.
exit /b 0
:fail
popd
exit /b 1
