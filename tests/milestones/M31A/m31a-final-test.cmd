@echo off
setlocal

echo === M31A FINAL static/import regression ===
call m31a-wmchar-test.cmd
exit /b %errorlevel%
