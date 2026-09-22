@echo off
echo === M28D2 OS/2 process/pipe regression ===
del m28d-direct.txt
del m28d-pipe.txt
del m28d-triple.txt
del m28d-builtin.txt

echo --- direct LX fixture sanity ---
..\emit-test.exe > m28d-direct.txt
type m28d-direct.txt

echo --- LX to LX ---
..\emit-test.exe | ..\upper-test.exe > m28d-pipe.txt
type m28d-pipe.txt

echo --- nested three-stage pipeline ---
..\emit-test.exe | ..\upper-test.exe | ..\upper-test.exe > m28d-triple.txt
type m28d-triple.txt

echo --- builtin shell worker to LX ---
echo mixedCase | ..\upper-test.exe > m28d-builtin.txt
type m28d-builtin.txt

del m28d-direct.txt
del m28d-pipe.txt
del m28d-triple.txt
del m28d-builtin.txt
echo M28D2 process/pipe regression complete
