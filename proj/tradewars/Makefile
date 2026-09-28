# File: Makefile (unix version)
# Simple Makefile to test a 
# hello world style program
# test each component of GCC
# Runs the pre-processor, the C compiler, the assembler, then the linker.

CC = cl386

OBJECTS =	combat.obj comptr.obj planet.obj port.obj tw4.obj
OSOBJ	= generic.obj
CONOBJ = condat.obj
MAILOBJ = mailman.obj 

CFLAGS = /I.
LDFLAGS = 

all: tw4 condat mailman test\twdata.dat test\player1.txt

tw4: $(OBJECTS) $(OSOBJ)
	$(CC) -v -o tw4 $(OBJECTS) $(OSOBJ) $(LDFLAGS)

condat: $(CONOBJ)
	$(CC) -v -o condat $(CONOBJ) $(LDFLAGS)

mailman: $(MAILOBJ)
	$(CC) -v -o mailman $(MAILOBJ) $(LDFLAGS)

test\twdata.dat: condat
	-condat.cmd

test\player1.txt: player1.txt
	-copy player1.txt test

firstrun: condat tw4 test\twdata.dat test\player1.txt
	-tw4.cmd player1.txt maint




clean:	
	@del /q *.obj
	@del /q *.exe
	@del /q tw4 condat mailman
	@del /q test\twdata.dat
	@del /q test\player1.txt