@echo off
rem M29P: build the reconstructed CMD32OS2 command processor plus batch-torture fixtures as an
rem OS/2 C/386 LE/LX executable using the real cmdos2_os2 backend.
rem
rem This is intentionally separate from build-os2-backend.cmd: M29I remains
rem the frozen backend regression, while this script exposes the next real
rem compatibility boundary in the reconstructed shell.

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

echo Compiling reconstructed CMD modules with Microsoft C/386...
cl386 /nologo /Gd /c cmdparse.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c cmdbatch.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c cmdfile.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c cmdos2_env.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c cmdos2_os2.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c cmdos2_session_os2.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c cmd32os2.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c rc-child.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c m29k-wait-child.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c m29k-detach-child.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c m29k3-break-child.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c m29k4-detach-child.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c cmdos2_os2_wait_test.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c cmdos2_os2_detach_test.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c m29l-session-child.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c cmdos2_os2_start_test.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c cmdos2_os2_startdata_test.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c emit-test.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c upper-test.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c m29m-stderr.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c m29m-count.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c m29n-args.c
if errorlevel 1 exit /b 1
cl386 /nologo /Gd /c cmd32-pskill-child.c
if errorlevel 1 exit /b 1

>m29j-shell.lnk echo cmd32os2.obj cmdparse.obj cmdbatch.obj cmdfile.obj cmdos2_os2.obj cmdos2_session_os2.obj cmdos2_env.obj
>>m29j-shell.lnk echo cmd32os2_os2.exe
>>m29j-shell.lnk echo cmd32os2_os2.map
>>m29j-shell.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29j-shell.lnk echo cmd32os2_os2.def

link386 @m29j-shell.lnk
if errorlevel 1 exit /b 1

>m29k-rc-child.lnk echo rc-child.obj
>>m29k-rc-child.lnk echo rc-child.exe
>>m29k-rc-child.lnk echo nul.map
>>m29k-rc-child.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29k-rc-child.lnk echo nul.def
link386 @m29k-rc-child.lnk
if errorlevel 1 exit /b 1

>m29k-wait-child.lnk echo m29k-wait-child.obj
>>m29k-wait-child.lnk echo m29k-wait-child.exe
>>m29k-wait-child.lnk echo nul.map
>>m29k-wait-child.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29k-wait-child.lnk echo nul.def
link386 @m29k-wait-child.lnk
if errorlevel 1 exit /b 1

>m29k-detach-child.lnk echo m29k-detach-child.obj
>>m29k-detach-child.lnk echo m29k-detach-child.exe
>>m29k-detach-child.lnk echo nul.map
>>m29k-detach-child.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29k-detach-child.lnk echo nul.def
link386 @m29k-detach-child.lnk
if errorlevel 1 exit /b 1

>m29k3-break-child.lnk echo m29k3-break-child.obj
>>m29k3-break-child.lnk echo m29k3-break-child.exe
>>m29k3-break-child.lnk echo nul.map
>>m29k3-break-child.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29k3-break-child.lnk echo nul.def
link386 @m29k3-break-child.lnk
if errorlevel 1 exit /b 1

>m29k-wait-test.lnk echo cmdos2_os2_wait_test.obj cmdos2_os2.obj cmdos2_env.obj
>>m29k-wait-test.lnk echo cmdos2_os2_wait_test.exe
>>m29k-wait-test.lnk echo nul.map
>>m29k-wait-test.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29k-wait-test.lnk echo cmdos2_os2_wait_test.def
link386 @m29k-wait-test.lnk
if errorlevel 1 exit /b 1

>m29k4-detach-child.lnk echo m29k4-detach-child.obj
>>m29k4-detach-child.lnk echo m29k4-detach-child.exe
>>m29k4-detach-child.lnk echo nul.map
>>m29k4-detach-child.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29k4-detach-child.lnk echo nul.def
link386 @m29k4-detach-child.lnk
if errorlevel 1 exit /b 1

>m29k4-detach-test.lnk echo cmdos2_os2_detach_test.obj cmdos2_os2.obj cmdos2_env.obj
>>m29k4-detach-test.lnk echo cmdos2_os2_detach_test.exe
>>m29k4-detach-test.lnk echo nul.map
>>m29k4-detach-test.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29k4-detach-test.lnk echo cmdos2_os2_detach_test.def
link386 @m29k4-detach-test.lnk
if errorlevel 1 exit /b 1


>m29l-session-child.lnk echo m29l-session-child.obj
>>m29l-session-child.lnk echo m29l-session-child.exe
>>m29l-session-child.lnk echo nul.map
>>m29l-session-child.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29l-session-child.lnk echo nul.def
link386 @m29l-session-child.lnk
if errorlevel 1 exit /b 1

>m29l-start-test.lnk echo cmdos2_os2_start_test.obj cmdos2_os2.obj cmdos2_session_os2.obj cmdos2_env.obj
>>m29l-start-test.lnk echo cmdos2_os2_start_test.exe
>>m29l-start-test.lnk echo nul.map
>>m29l-start-test.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29l-start-test.lnk echo cmdos2_os2_start_test.def
link386 @m29l-start-test.lnk
if errorlevel 1 exit /b 1

>m29l1-startdata-test.lnk echo cmdos2_os2_startdata_test.obj cmdos2_os2.obj cmdos2_session_os2.obj cmdos2_env.obj
>>m29l1-startdata-test.lnk echo cmdos2_os2_startdata_test.exe
>>m29l1-startdata-test.lnk echo nul.map
>>m29l1-startdata-test.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29l1-startdata-test.lnk echo cmdos2_os2_startdata_test.def
link386 @m29l1-startdata-test.lnk
if errorlevel 1 exit /b 1


>cmd32-pskill-child.lnk echo cmd32-pskill-child.obj
>>cmd32-pskill-child.lnk echo cmd32-pskill-child.exe
>>cmd32-pskill-child.lnk echo nul.map
>>cmd32-pskill-child.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>cmd32-pskill-child.lnk echo cmd32-pskill-child.def
link386 @cmd32-pskill-child.lnk
if errorlevel 1 exit /b 1

>m29m-emit.lnk echo emit-test.obj
>>m29m-emit.lnk echo emit-test.exe
>>m29m-emit.lnk echo nul.map
>>m29m-emit.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29m-emit.lnk echo nul.def
link386 @m29m-emit.lnk
if errorlevel 1 exit /b 1

>m29m-upper.lnk echo upper-test.obj
>>m29m-upper.lnk echo upper-test.exe
>>m29m-upper.lnk echo nul.map
>>m29m-upper.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29m-upper.lnk echo nul.def
link386 @m29m-upper.lnk
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

>m29n-args.lnk echo m29n-args.obj
>>m29n-args.lnk echo m29n-args.exe
>>m29n-args.lnk echo nul.map
>>m29n-args.lnk echo %OS2LIB%\LIBC.LIB %OS2LIB%\OS2386.LIB
>>m29n-args.lnk echo nul.def
link386 @m29n-args.lnk
if errorlevel 1 exit /b 1

if not exist m29n-bin mkdir m29n-bin
if not exist "m29n path bin" mkdir "m29n path bin"
if not exist "m29n space" mkdir "m29n space"
copy /Y m29n-args.exe m29n-bin\pathprobe.exe >nul
if errorlevel 1 exit /b 1
copy /Y m29n-args.exe m29n-bin\pathpick.exe >nul
if errorlevel 1 exit /b 1
copy /Y m29n-args.exe "m29n path bin\qpathprobe.exe" >nul
if errorlevel 1 exit /b 1
copy /Y m29n-args.exe "m29n space\quoted args.exe" >nul
if errorlevel 1 exit /b 1

echo.
echo Built cmd32os2_os2.exe: reconstructed CMD32OS2 linked as a C/386 OS/2 image.
echo First scan: os2host32.exe --scan cmd32os2_os2.exe
echo Scripted:   os2host32.exe --run cmd32os2_os2.exe -c "echo M29P_SHELL_OK"
echo Interactive: os2host32.exe --run cmd32os2_os2.exe
echo Wait test:   os2host32.exe --run cmdos2_os2_wait_test.exe
echo Errorlevel:  run examples\m29k2-errorlevel-test.cmd inside CMD32OS2
echo Detach API:  os2host32.exe --run cmdos2_os2_detach_test.exe
echo Detach:      run examples\m29k4-detach-hardening.cmd inside CMD32OS2

echo Ctrl+C:     run m29k3-break-child then press Ctrl+C; shell should survive and ERRORLEVEL should be 4

echo START API:   os2host32.exe --run cmdos2_os2_start_test.exe
echo STARTDATA:   set OS2_TRACE_SESSION=1 then os2host32.exe --run cmdos2_os2_startdata_test.exe
echo START shell: set OS2_TRACE_SESSION=1 then run examples\m29l1-startdata-test.cmd
echo PS/KILL:     START cmd32-pskill-child, then PS, then KILL task-id
echo M29M redir: run examples\m29m-redir-pipe-test.cmd inside CMD32OS2
echo M29N lookup: set CMD32_TRACE_RESOLVE=1 then run examples\m29n-command-resolution-test.cmd
echo M29N1 path/argv0: run examples\m29n1-os2path-argv0-test.cmd

echo M29O1 batch: run examples\m29o-batch-torture.cmd from root OR cd examples then run m29o-batch-torture.cmd
echo M29O1 Ctrl+C: run examples\m29o-ctrlc-batch-test.cmd and press Ctrl+C once
echo M29P files: run examples\m29p-filesystem-test.cmd from root OR cd examples then run m29p-filesystem-test.cmd
