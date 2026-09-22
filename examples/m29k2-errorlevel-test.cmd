@echo off
..\rc-child.exe 37
echo errorlevel=%errorlevel%
if errorlevel 37 echo IF_ERRORLEVEL_OK
echo preserved=%errorlevel%
if not errorlevel 38 echo IF_NOT_ERRORLEVEL_OK
if exist ..\rc-child.exe echo IF_EXIST_OK
if "same"=="same" echo IF_COMPARE_OK
for %i in (one two three) do echo FOR=%i
