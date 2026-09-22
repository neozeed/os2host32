# Makefile for OPENDLG.DLL
# Created by Microsoft Corporation, 1989
#

# Abbreviations
#
ASM	= masm386 /l /ml
CC	= cl386 -Zip -W3 -c -Gs -Oi -NT
LF	= /far /map /nod /codeview
OBJS	= tool.obj tool1.obj toola.obj data.obj\
	  file.obj file1.obj init.obj gpi.obj

# Inference Rules
#
.asm.obj:
    $(ASM) $*.asm;

.c.obj:
    $(CC) _TEXT $*.c

# Resources
#
all:	opendlg.lib opendlg.dll

opendlg.res:	opendlg.dlg opendlg.h opendlg.rc tool.h
    rc -r opendlg.rc

# Import Libraries
#
opendlg.lib:	opendlg.def
    implib $*.lib $*.def

# MASM files
#
toola.obj:	toola.asm

# C files
#
tool.obj:	tool.c

data.obj:	data.c
    $(CC) _INIT data.c

gpi.obj:	gpi.c
    $(CC) _INIT gpi.c

tool1.obj:	tool1.c
    $(CC) _INIT tool1.c

init.obj:	init.c
    $(CC) _INIT init.c

file.obj:	file.c
    $(CC) _FILE file.c

file1.obj:	file1.c
    $(CC) _FILE file1.c

# Main files:  OPENDLG.DLL
#
opendlg.lnk:	opendlg.mak
    echo $(OBJS)	>  opendlg.lnk
    echo opendlg.dll	>> opendlg.lnk
    echo opendlg.map	>> opendlg.lnk
    echo opendlg os2386	>> opendlg.lnk
    echo opendlg.def	>> opendlg.lnk

opendlg.dll:	opendlg.lnk opendlg.def opendlg.res $(OBJS)
    link386 $(LF) @opendlg.lnk
    rc opendlg.res opendlg.dll
