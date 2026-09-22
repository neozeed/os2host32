@echo off
echo === M29N command lookup + quoting regression ===
set M29N_OLDPATH=%PATH%
set PATH="m29n path bin";m29n-bin;%M29N_OLDPATH%

echo --- extensionless executable through PATH; argv0 stays typed ---
pathprobe alpha

echo --- explicit .EXE through PATH ---
pathprobe.exe beta

echo --- quoted PATH element containing spaces ---
qpathprobe "path spaced"

echo --- quoted explicit program path + spaces + empty argument ---
"m29n space\quoted args" "one two" "" three

echo --- executable beats CMD in the same PATH directory ---
pathpick precedence

echo --- extensionless CMD fallback through PATH ---
onlycmd batcharg

echo --- current directory is searched before PATH directories ---
m29n-shadow

set PATH=%M29N_OLDPATH%
set M29N_OLDPATH=
echo M29N_COMMAND_RESOLUTION_OK
