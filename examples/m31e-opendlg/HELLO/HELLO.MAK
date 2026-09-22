# Makefile for HELLO.EXE
# Created by Microsoft, IBM Corporation 1990
#

# Abbreviations
#
CC	=	cl386 -c -W3 -Asnd -Gs -Oi -Zip -I.. -DLINT_ARGS
LF	=	/codeview /NOD /map

# Inference Rules
#
.c.obj:
    $(CC) $*.c

.rc.res:
    rc -r $*.rc

all:	hello.exe

# Resources
#
hello.res:	hello.h hello.ico hello.rc

# C files
#
hello.obj:	hello.c hello.h

# Main files:  HELLO.EXE
#
hello.exe:	hello.mak hello.def hello.obj hello.res
    link386 $(LF) hello, hello, hello, ..\opendlg os2386 libc, hello.def
    rc hello.res hello.exe
