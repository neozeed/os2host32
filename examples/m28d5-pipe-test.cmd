@echo off
rem M28D5 pipeline test using an OS/2 DosRead/DosWrite filter, not C stdio.
del m28d5-pipe.txt
del m28d5-triple.txt
del m28d5-builtin.txt

echo === M28D5 OS/2 process/pipe regression ===
echo --- LX to LX (direct OS/2 HFILE filter) ---
..\emit-test.exe | ..\upper-dos-test.exe > m28d5-pipe.txt
type m28d5-pipe.txt

echo --- nested three-stage pipeline ---
..\emit-test.exe | ..\upper-dos-test.exe | ..\upper-dos-test.exe > m28d5-triple.txt
type m28d5-triple.txt

echo --- builtin shell worker to LX ---
echo mixedCase | ..\upper-dos-test.exe > m28d5-builtin.txt
type m28d5-builtin.txt

echo M28D5 process/pipe regression complete
