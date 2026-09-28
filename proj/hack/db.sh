rm -f hack
make
cd save
uudecode 1000jsteve.uue
cd ..
gdb hack
