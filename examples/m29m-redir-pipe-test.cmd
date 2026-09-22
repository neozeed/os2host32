@echo off
echo === M29M descriptor redirection + pipeline regression ===
del m29m-out.txt 2>nul
del m29m-err.txt 2>nul
del m29m-both.txt 2>nul
del m29m-append.txt 2>nul
del m29m-order.txt 2>nul
del m29m-upper.txt 2>nul
del m29m-count.txt 2>nul
del m29m-pipe-both.txt 2>nul

echo --- split stdout/stderr ---
m29m-stderr >m29m-out.txt 2>m29m-err.txt
echo stdout-file:
type m29m-out.txt
echo stderr-file:
type m29m-err.txt

echo --- merge stdout+stderr ---
m29m-stderr >m29m-both.txt 2>&1
type m29m-both.txt

echo --- append stderr twice ---
m29m-stderr 2>m29m-append.txt >nul
m29m-stderr 2>>m29m-append.txt >nul
type m29m-append.txt

echo --- ordering: stderr stays inherited, stdout goes to file ---
m29m-stderr 2>&1 >m29m-order.txt
echo order-file-should-contain-stdout-only:
type m29m-order.txt

echo --- three-stage concurrent pipeline ---
emit-test | upper-test | upper-test >m29m-upper.txt
type m29m-upper.txt
emit-test | upper-test | m29m-count >m29m-count.txt
type m29m-count.txt

echo --- merge stderr into a pipe ---
m29m-stderr 2>&1 | upper-test >m29m-pipe-both.txt
type m29m-pipe-both.txt

echo M29M_REDIRECTION_PIPELINE_OK
