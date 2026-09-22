@echo off
del m29k4-detach.ok
del m29k4-detach.out
set M29K4_TOKEN=DETACH_ENV_OK
detach m29k4-detach-child.exe m29k4-detach.ok > m29k4-detach.out
echo DETACH_RETURNED=%ERRORLEVEL%
rem Give the detached child time to finish without waiting on that child.
m29k-wait-child.exe
type m29k4-detach.ok
type m29k4-detach.out
