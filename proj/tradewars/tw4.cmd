@rem MS-DOS DJGPP v2 CSDPMI driver thing
@IF NOT EXIST tw4.exe (
echo.
echo Executable missing attempting to compile....
@make -f makefile tw4
echo.
)
cd test
@..\..\..\bin\msdos.exe ..\tw4.exe %1 %2
cd ..