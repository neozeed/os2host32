
INC = /u /w
OPT = /G3 /O
DEBUG = #/Zi
LDEBUG = #-debug:full

# native CC
CC =  cl386

OS2LINK = LINK386.EXE


OBJ = astro.obj date_p.obj phoon.obj



phoon2.exe: $(OBJ)
	$(OS2LINK) @phoon.lnk

clean:
	del $(OBJ)
	del phoon.exe phoon2.exe
	del *.asm *.a1 *.a

