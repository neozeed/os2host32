@echo off
rc-child.exe 37
echo ERRORLEVEL_AFTER_37=%ERRORLEVEL%
rc-child.exe 37
if errorlevel 37 echo IF_ERRORLEVEL_GE_37_OK
rc-child.exe 37
if not errorlevel 38 echo IF_ERRORLEVEL_LT_38_OK
rc-child.exe 0
echo ERRORLEVEL_AFTER_0=%ERRORLEVEL%
