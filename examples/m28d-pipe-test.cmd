@echo off
echo === M28D OS/2 process/pipe regression ===
del m28d-pipe.txt
del m28d-triple.txt
del m28d-builtin.txt

echo --- LX to LX ---
..\emit-test.exe | ..\upper-test.exe > m28d-pipe.txt
type m28d-pipe.txt

echo --- nested three-stage pipeline ---
..\emit-test.exe | ..\upper-test.exe | ..\upper-test.exe > m28d-triple.txt
type m28d-triple.txt

echo --- builtin shell worker to LX ---
echo mixedCase | ..\upper-test.exe > m28d-builtin.txt
type m28d-builtin.txt

del m28d-pipe.txt
del m28d-triple.txt
del m28d-builtin.txt
echo M28D process/pipe regression complete
