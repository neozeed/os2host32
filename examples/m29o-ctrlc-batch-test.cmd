@echo off
echo === M29O manual Ctrl+C-in-batch regression ===
echo The child will wait. Press Ctrl+C once.
m29k3-break-child.exe
echo M29O_CTRL_C_ERRORLEVEL=%ERRORLEVEL%
if errorlevel 4 if not errorlevel 5 echo M29O_CTRL_C_RESULT_OK
echo M29O_CTRL_C_BATCH_CONTINUED_OK
