# Milestone 29K2 - native environment pseudo-variables + stable ERRORLEVEL

M29K1 removed the C/386 nested-parser stack overflow.  User testing then
identified two independent shell-semantic issues:

1. The native Win32 bootstrap (`cmd32os2.exe`) inherited hidden Windows
   current-directory pseudo-variables such as `=::=::\\`.  M29J2's environment
   validator only allowed the familiar `=C:=C:\\path` spelling, rejected that
   legal inherited entry, and returned OS/2 rc=87 before CreateProcessA.
2. `echo %errorlevel%` displayed the correct value but ECHO's ordinary success
   return then overwrote ShellState::last_rc with zero.  Consequently a
   following `IF ERRORLEVEL n` observed the wrong state.

M29K2 fixes both without changing the LE/LX loader or the seven C/386 far16
console bridges.

## Win32 environment boundary

The DOSCALLS normalizer now accepts hidden environment entries generically:

    =<hidden-name>=<value>

The initial '=' is part of the pseudo-variable name; a later '=' is the
separator.  This covers both `=C:=...` and entries such as `=::=::\\`.  They
remain part of the sorted custom environment passed to CreateProcessA.

## ERRORLEVEL ownership

The command engine now separates a command's execution return from whether it
owns the persistent ERRORLEVEL state.  ECHO and REM preserve the prior value.
IF/FOR/CALL preserve the state established by any nested command they execute.
External programs and ordinary result-producing built-ins still update it.

This means the following is now intended to retain 37 throughout:

    rc-child 37
    echo %errorlevel%
    if errorlevel 37 echo IF_ERRORLEVEL_OK
    echo %errorlevel%

ECHO itself still returns success to the parser, so `echo ok && echo next`
continues to behave as a successful command chain.

## Regression

From the hosted C/386 shell:

    rc-child 37
    echo %errorlevel%
    if errorlevel 37 echo YES
    echo %errorlevel%

Expected:

    37
    YES
    37

For the native bootstrap, enable tracing and launch either child:

    set OS2_TRACE_EXEC=1
    cmd32os2.exe
    rc-child 37

or:

    cd ..\sar
    sarienlx

The old diagnostic:

    DOSCALLS EXEC: invalid environment entry [=::=::\\]

must be gone.  The nested OS2HOST32 command should instead be shown and the
child should run.

The batch regression under `examples` now invokes `..\\rc-child.exe`, because
when the script is run from the examples directory the fixture is one level up.
