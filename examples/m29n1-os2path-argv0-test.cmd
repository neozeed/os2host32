@echo off
echo === M29N1 OS/2 PATH namespace + argv0 regression ===

set M29N1_OLD_OS2PATH=%OS2PATH%

echo --- SET keeps literal parentheses in data ---
set M29N1_PAREN=C:\Program Files (x86)\Tools
set M29N1_PAREN

echo --- OS2PATH drives the guest PATH ---
set OS2PATH="m29n path bin";m29n-bin
path

echo --- extensionless PATH resolution preserves typed argv0 ---
pathprobe alpha

echo --- quoted PATH directory preserves typed argv0 ---
qpathprobe "path spaced"

echo --- explicit quoted path preserves typed argv0 + empty argument ---
"m29n space\quoted args" "one two" "" three

set OS2PATH=%M29N1_OLD_OS2PATH%
set M29N1_OLD_OS2PATH=
set M29N1_PAREN=

echo M29N1_OS2PATH_ARGV0_OK
