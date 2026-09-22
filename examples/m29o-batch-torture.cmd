@echo off
echo === M29O1 batch-language torture regression ===

set M29O_FAIL=0
set M29O_SCOPE=outer
set M29O_EXDIR=
set M29O_ROOT=
if not exist m29o-child.cmd set M29O_EXDIR=examples\
if not exist rc-child.exe set M29O_ROOT=..\

rem Positional parameters, quoted/empty args, SHIFT, %*, CALL second pass.
call :args alpha "beta gamma" "" delta

rem Nested CALL :label frames.
call :outer_sub top

rem SETLOCAL must unwind when a subroutine returns via GOTO :EOF.
call :scope_sub
if "%M29O_SCOPE%"=="outer" echo M29O_SUB_SETLOCAL_UNWIND_OK
if not "%M29O_SCOPE%"=="outer" set M29O_FAIL=1

rem A called child batch gets its own SETLOCAL lifetime and preserves args.
call %M29O_EXDIR%m29o-child.cmd first "two words" "" fourth
if errorlevel 1 set M29O_FAIL=1
if "%M29O_SCOPE%"=="outer" echo M29O_CHILD_SETLOCAL_UNWIND_OK
if not "%M29O_SCOPE%"=="outer" set M29O_FAIL=1

rem CALLed batch ERRORLEVEL must propagate back to the caller.
call %M29O_EXDIR%m29o-rc-child.cmd %M29O_ROOT%rc-child.exe
if errorlevel 24 set M29O_FAIL=1
if not errorlevel 23 set M29O_FAIL=1
if errorlevel 23 if not errorlevel 24 echo M29O_CALL_ERRORLEVEL_OK

rem Nested IF and the old >= ERRORLEVEL rule.
%M29O_ROOT%rc-child.exe 37
if errorlevel 38 set M29O_FAIL=1
if not errorlevel 37 set M29O_FAIL=1
if errorlevel 37 if not errorlevel 38 echo M29O_NESTED_IF_OK
if not exist __m29o_definitely_missing__.tmp echo M29O_IF_NOT_EXIST_OK
if "same"=="same" echo M29O_IF_STRING_OK

rem Nested FOR: outer substitution must leave the inner variable intact.
set M29O_FOR_ONE_A=
set M29O_FOR_ONE_B=
set M29O_FOR_TWO_A=
set M29O_FOR_TWO_B=
for %%i in (one two) do for %%j in (A B) do call :for_check %%i %%j
if not "%M29O_FOR_ONE_A%"=="1" set M29O_FAIL=1
if not "%M29O_FOR_ONE_B%"=="1" set M29O_FAIL=1
if not "%M29O_FOR_TWO_A%"=="1" set M29O_FAIL=1
if not "%M29O_FOR_TWO_B%"=="1" set M29O_FAIL=1
if "%M29O_FOR_ONE_A%"=="1" if "%M29O_FOR_ONE_B%"=="1" if "%M29O_FOR_TWO_A%"=="1" if "%M29O_FOR_TWO_B%"=="1" echo M29O_NESTED_FOR_OK

rem GOTO and label lookup.
goto after_goto
echo M29O_GOTO_FAILED_SHOULD_NOT_PRINT
set M29O_FAIL=1
:after_goto
echo M29O_GOTO_OK

rem Invoking a batch file without CALL replaces that batch frame.  The wrapper
rem is itself CALLed so this top-level torture script can verify the return.
set M29O_CHAIN_PARENT_BEFORE_FLAG=
set M29O_CHAIN_TARGET_OK_FLAG=
set M29O_CHAIN_BAD=
call %M29O_EXDIR%m29o-chain-parent.cmd %M29O_EXDIR%m29o-chain-target.cmd
if not "%M29O_CHAIN_PARENT_BEFORE_FLAG%"=="1" set M29O_FAIL=1
if not "%M29O_CHAIN_TARGET_OK_FLAG%"=="1" set M29O_FAIL=1
if "%M29O_CHAIN_BAD%"=="1" set M29O_FAIL=1
if "%M29O_CHAIN_PARENT_BEFORE_FLAG%"=="1" if "%M29O_CHAIN_TARGET_OK_FLAG%"=="1" if not "%M29O_CHAIN_BAD%"=="1" echo M29O_CHAIN_RETURNED_TO_CALLER_OK

if not "%M29O_FAIL%"=="0" echo M29O_BATCH_TORTURE_FAILED
if "%M29O_FAIL%"=="0" echo M29O_BATCH_TORTURE_OK
goto :eof

:args
echo M29O_ARGS0=[%0]
echo M29O_ARGS1=[%1]
echo M29O_ARGS2=[%2]
echo M29O_ARGS3=[%3]
echo M29O_ARGS4=[%4]
echo M29O_STAR_BEFORE=[%*]
call echo M29O_CALL_SECOND_PASS=[%%1]
shift
echo M29O_SHIFT1=[%1]
echo M29O_SHIFT2=[%2]
echo M29O_SHIFT3=[%3]
echo M29O_STAR_AFTER=[%*]
goto :eof

:outer_sub
echo M29O_OUTER_SUB=[%1]
call :inner_sub inner
if "nested"=="nested" echo M29O_NESTED_CALL_RETURN_OK
goto :eof

:inner_sub
echo M29O_INNER_SUB=[%1]
goto :eof

:scope_sub
setlocal
set M29O_SCOPE=inner
echo M29O_SUB_SCOPE_INSIDE=%M29O_SCOPE%
goto :eof

:for_check
echo M29O_NESTED_FOR=%1-%2
if "%1-%2"=="one-A" set M29O_FOR_ONE_A=1
if "%1-%2"=="one-B" set M29O_FOR_ONE_B=1
if "%1-%2"=="two-A" set M29O_FOR_TWO_A=1
if "%1-%2"=="two-B" set M29O_FOR_TWO_B=1
goto :eof
