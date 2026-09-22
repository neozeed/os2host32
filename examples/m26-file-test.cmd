@echo off
rem M26 cfile.c regression - run from a disposable test directory.

echo === M26 cfile regression ===
md m26work
md m26work\dest

echo alpha>m26work\one.txt
echo beta>m26work\two.txt

echo --- COPY single ---
copy m26work\one.txt m26work\copy.txt
type m26work\copy.txt

echo --- RENAME ---
ren m26work\copy.txt renamed.txt
type m26work\renamed.txt

echo --- MOVE ---
move m26work\renamed.txt m26work\dest
type m26work\dest\renamed.txt

echo --- COPY wildcard to directory ---
copy m26work\*.txt m26work\dest
dir m26work\dest\*.txt

echo --- DEL wildcard ---
del m26work\dest\*.txt
dir m26work\dest\*.txt

echo --- cleanup ---
del m26work\*.txt
rd m26work\dest
rd m26work

echo M26 file-command regression complete
