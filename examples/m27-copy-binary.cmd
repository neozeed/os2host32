@echo off
rem M27 COPY /B and binary concatenation regression.

echo === M27 COPY /B regression ===
copy /b m27-part1.bin m27-copy.bin
copy /b m27-part1.bin+m27-part2.bin m27-combined.bin

echo --- resulting sizes ---
dir m27-copy.bin
dir m27-combined.bin

echo Expected sizes: 10 bytes and 18 bytes.
echo From ordinary Windows CMD verify exact bytes with:
echo   fc /b m27-combined.bin m27-expected.bin
