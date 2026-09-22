# Makefile for BIOrhythm program
# Created by Microsoft, IBM Corporation, 1990
#

all: bio.exe

# Abbreviations
#
CC	=	cl386 -c -Gs -Od -W3 -Zi
LF	=	/codeview /map /NOD
OBJS	=	bio.obj biocmd.obj biopaint.obj wndproc.obj

# Inference Rules
#
.c.obj:
    $(CC) $*.c

.rc.res:
    rc -r $*.rc

# Dependencies
#
bio.res:	bio.h bio.ico bio.rc

bio.obj:	bio.c bio.h

biocmd.obj:	bio.h biocmd.c

biopaint.obj:	bio.h biopaint.c

wndproc.obj:	bio.h wndproc.c

bio.exe:	bio.mak $(OBJS) bio.res bio.def
    link386 $(LF) $(OBJS), bio.exe, bio.map, os2386 libc, bio.def
    rc bio.res
