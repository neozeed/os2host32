# Makefile for WMCHAR application
# Created by Microsoft, IBM Corporation 1990
#

# Abbreviations
#
CC = cl386 -c -Gs -Oi -W3 -Zip
LF = /codeview /map /NOD

# Inference Rules
#
.c.obj:
    $(CC) $*.c

.rc.res:
    rc -r $*.rc

all:	wmchar.exe

# Dependencies
#
wmchar.res: wmchar.h wmchar.ico wmchar.rc

wmchar.obj: wmchar.h wmchar.c

wmchar.exe: wmchar.res wmchar.obj
    link386 $(LF) wmchar, , , os2386 libc, wmchar.def
    rc wmchar.res
