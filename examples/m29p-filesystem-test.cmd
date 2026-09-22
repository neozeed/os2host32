@echo off
rem M29P final built-in/filesystem compatibility regression.

echo === M29P filesystem + small-stack regression ===

set M29P_ROOT=.
if exist cmd32os2_os2.exe goto :m29p_root_ready
if exist ..\cmd32os2_os2.exe set M29P_ROOT=..
:m29p_root_ready
if not exist %M29P_ROOT%\cmd32os2_os2.exe goto :m29p_fail_root

rem Best-effort cleanup from an interrupted older run.
del "m29pwork\space dir\*" >nul 2>&1
del m29pwork\* >nul 2>&1
rd "m29pwork\space dir" >nul 2>&1
rd m29pwork >nul 2>&1

md m29pwork
if errorlevel 1 goto :m29p_fail
md "m29pwork\space dir"
if errorlevel 1 goto :m29p_fail

echo alpha>m29pwork\one.txt
echo beta>m29pwork\two.txt
echo move-me>m29pwork\moveme.dat

rem This COPY used to enter a function with more than 260 KiB of automatic
rem token/source storage, before its 32 KiB transfer buffer was counted.
copy /b %M29P_ROOT%\cmd32os2_os2.exe m29pwork\large-copy.exe
if errorlevel 1 goto :m29p_fail
if not exist m29pwork\large-copy.exe goto :m29p_fail
 echo M29P_HEAP_COPY_OK

rem Common DOS/OS2 wildcard rename transformation.
ren m29pwork\*.txt *.bak
if errorlevel 1 goto :m29p_fail
if exist m29pwork\one.txt goto :m29p_fail
if exist m29pwork\two.txt goto :m29p_fail
if not exist m29pwork\one.bak goto :m29p_fail
if not exist m29pwork\two.bak goto :m29p_fail
 echo M29P_WILDCARD_RENAME_OK

rem COPY to a quoted directory and DIR /B.
copy m29pwork\*.bak "m29pwork\space dir"
if errorlevel 1 goto :m29p_fail
if not exist "m29pwork\space dir\one.bak" goto :m29p_fail
if not exist "m29pwork\space dir\two.bak" goto :m29p_fail
 echo --- M29P DIR /B output ---
dir /b "m29pwork\space dir\*.bak"
if errorlevel 1 goto :m29p_fail
 echo M29P_DIR_BARE_OK

rem Self-copy must fail without truncating/deleting the source.
copy m29pwork\one.bak m29pwork\one.bak
if not errorlevel 1 goto :m29p_fail
if not exist m29pwork\one.bak goto :m29p_fail
 echo M29P_SELF_COPY_GUARD_OK

rem Binary concatenation remains intact after moving COPY workspace to heap.
echo part-one>m29pwork\part1.bin
echo part-two>m29pwork\part2.bin
copy /b m29pwork\part1.bin+m29pwork\part2.bin m29pwork\combined.bin
if errorlevel 1 goto :m29p_fail
if not exist m29pwork\combined.bin goto :m29p_fail
 echo M29P_COPY_CONCAT_OK

move m29pwork\moveme.dat "m29pwork\space dir"
if errorlevel 1 goto :m29p_fail
if exist m29pwork\moveme.dat goto :m29p_fail
if not exist "m29pwork\space dir\moveme.dat" goto :m29p_fail
 echo M29P_MOVE_OK

type m29pwork\one.bak
if errorlevel 1 goto :m29p_fail
 echo M29P_TYPE_OK

rem Wildcard DEL and directory removal.
del "m29pwork\space dir\*"
if errorlevel 1 goto :m29p_fail
rd "m29pwork\space dir"
if errorlevel 1 goto :m29p_fail
del m29pwork\*
if errorlevel 1 goto :m29p_fail
rd m29pwork
if errorlevel 1 goto :m29p_fail

 echo M29P_FILESYSTEM_OK
goto :eof

:m29p_fail_root
echo M29P_FILESYSTEM_FAILED: cannot locate cmd32os2_os2.exe
goto :eof

:m29p_fail
echo M29P_FILESYSTEM_FAILED
goto :eof
