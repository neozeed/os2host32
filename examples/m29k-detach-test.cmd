@echo off
del m29k-detach.ok
detach m29k-detach-child.exe m29k-detach.ok
echo DETACH_RETURNED=%ERRORLEVEL%
rem Use a synchronous sleeper so the detached child has time to write its marker.
m29k-wait-child.exe
type m29k-detach.ok
