@echo off
if exist m29l-start-shell.ok del m29l-start-shell.ok
start m29l-session-child m29l-start-shell.ok
echo START_RETURNED=%errorlevel%
m29k-wait-child
if exist m29l-start-shell.ok echo M29L_START_MARKER_OK
if not exist m29l-start-shell.ok echo M29L_START_MARKER_MISSING
if exist m29l-start-shell.ok type m29l-start-shell.ok
if exist m29l-start-shell.ok del m29l-start-shell.ok
