@echo off
echo === M29A VIO/KBD console regression ===
echo PAUSE should wait for one keyboard event through KBDCALLS.
pause
echo PAUSE returned.
echo CLS should clear through VIOCALLS and leave this line at the top.
cls
echo M29A VIO/KBD console regression complete
