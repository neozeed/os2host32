#===================================================================
#
#   Sample Make file
# Created by Microsoft, IBM Corporation, 1990
#
#===================================================================

.SUFFIXES: .res .rc

ASM = masm
AFLAGS =
AXFLAGS =
CC = cl386
CFLAGS16 = -u -c -Gs -Od -W3 -Zpei -AL
CFLAGS32 = -c -Gs -Od -W3 -Zi -B1 c1l_386
CINC32 = 
CINC16 = -X -I\sdk12\inc -I\sdk12\inc\sys -I..\..\inc
DFLAGS =
LINK16 = link
LIBS16 = os2 llibcep printdlg
LINK32 = link386
LIBS32 = os2386 libc
LFLAGS = /MAP/CO/NOD
RC = ..\..\..\os2bin\rc
HEADERS = cheap.h globals.h jigsaw.h opendlg.h statwnd.h

LINK = $(LINK32)
CFLAGS = $(CFLAGS32)
LIBS = $(LIBS32)
CINC = $(CINC32)
#===================================================================
#
#   Default inference rules
#
#===================================================================
.c.obj:
    $(CC) $(CFLAGS) $(CINC) $*.c

.asm.obj:
    $(ASM) $(AFLAGS) $(AXFLAGS) $*.asm;

.rc.res:
    $(RC) -r $*.rc

#===================================================================
#
#   A list of all of the object files
#
#===================================================================
OBJECTS=jigsaw.obj procs.obj misc.obj globals.obj

#===================================================================
#
#   A list of all of the Help files
#
#===================================================================
ALL_IPF = 

#===================================================================
#
#   Dependencies
#
#===================================================================

all: jigsaw.exe

jigsaw.lnk: jigsaw.mak
    echo $(OBJECTS)	      > jigsaw.lnk
    echo jigsaw.exe           >> jigsaw.lnk
    echo jigsaw.map $(LFLAGS) >> jigsaw.lnk
    echo $(LIBS)	      >> jigsaw.lnk
    echo jigsaw.def           >> jigsaw.lnk

jigsaw.res: jigsaw.rc jigsaw.h statwnd.dlg cheap.dlg jigsaw.ico

jigsaw.obj: jigsaw.c $(HEADERS)

procs.obj: procs.c $(HEADERS)

misc.obj: misc.c $(HEADERS)

globals.obj: globals.c $(HEADERS)


#=============== help file generation ====================#
#jigsaw.hlp: $(ALL_IPF)
#    ipfc jigsaw.ipf /W3

jigsaw.exe: $(OBJECTS) jigsaw.def jigsaw.lnk jigsaw.res
    $(LINK) @jigsaw.lnk
    $(RC) jigsaw.res jigsaw.exe
