@echo off
if exist m29l1-parent.ok del m29l1-parent.ok
if exist m29l1-shellenv.ok del m29l1-shellenv.ok
if exist m29l1-pm.ok del m29l1-pm.ok
set M29L_TOKEN=PARENT_ENV_OK
start "M29L1 titled VIO" /win /min /pgm m29l-session-child.exe m29l1-parent.ok
echo START_WIN_RETURNED=%errorlevel%
m29k-wait-child.exe
type m29l1-parent.ok
start "M29L1 shell environment" /i /pgm m29l-session-child.exe m29l1-shellenv.ok
echo START_I_RETURNED=%errorlevel%
m29k-wait-child.exe
type m29l1-shellenv.ok
start "M29L1 PM session" /pm /pgm m29l-session-child.exe m29l1-pm.ok
echo START_PM_RETURNED=%errorlevel%
m29k-wait-child.exe
type m29l1-pm.ok
if exist m29l1-parent.ok del m29l1-parent.ok
if exist m29l1-shellenv.ok del m29l1-shellenv.ok
if exist m29l1-pm.ok del m29l1-pm.ok
