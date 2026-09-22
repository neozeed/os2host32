@echo off
setlocal
set M29O_SCOPE=child
echo M29O_CHILD_ARGS1=[%1]
echo M29O_CHILD_ARGS2=[%2]
echo M29O_CHILD_ARGS3=[%3]
echo M29O_CHILD_ARGS4=[%4]
echo M29O_CHILD_STAR=[%*]
shift
echo M29O_CHILD_SHIFT1=[%1]
echo M29O_CHILD_SCOPE_INSIDE=%M29O_SCOPE%
rem Deliberately no ENDLOCAL: EOF must unwind this child batch's localization.
