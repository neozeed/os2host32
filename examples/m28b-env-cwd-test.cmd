@echo off
echo === M28B environment/current-directory regression ===
set M28BTEST=outer
echo initial=%M28BTEST%
setlocal
set M28BTEST=inner
echo local=%M28BTEST%
endlocal
echo restored=%M28BTEST%
set M28BTEST=
echo cleared=[%M28BTEST%]

echo --- pipeline worker inherits CMD environment ---
set M28BTEST=pipe-worker
echo ignored | m28b-pipe-env.cmd
set M28BTEST=

echo --- current directory ---
md m28bcd
cd m28bcd
cd
cd ..
rd m28bcd

echo --- PATH through CMD-owned environment ---
md m28bpath
echo echo PATH-LOOKUP-OK > m28bpath\pathtest.cmd
setlocal
path m28bpath;%PATH%
pathtest
endlocal
del m28bpath\pathtest.cmd
rd m28bpath

echo M28B environment/current-directory regression complete
