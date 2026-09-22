@echo off
del m29k4-survive.ok
set M29K4_TOKEN=PARENT_EXIT_OK
detach m29k4-detach-child.exe m29k4-survive.ok
exit
