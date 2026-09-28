@rem MS-DOS DJGPP v2 CSWDPMI driver thing
@IF NOT EXIST condat.exe (
echo.
echo Executable missing attempting to compile....
@make -f makefile condat
echo.
)
@copy /Y twdata.txt test
@cd test
@..\..\..\bin\msdos.exe ..\condat.exe %1 %2
@cd ..
@del /F test\twdata.txt