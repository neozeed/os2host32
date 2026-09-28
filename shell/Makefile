
# simple makefile for the OS/2 version of cmd.exe

CC = cl386

CFLAGS = /nologo /Gd

OBJS = cmd32os2.obj cmdparse.obj cmdbatch.obj cmdfile.obj cmdos2_os2.obj cmdos2_session_os2.obj cmdos2_env.obj

CONOBJS = cmdos2_os2_console_test.obj cmdos2_os2.obj cmdos2_env.obj

all: cmd32os2_os2.exe cmdos2_os2_console_test.exe

cmd32os2_os2.exe: $(OBJS)
	link386 @cmd32.lnk

cmdos2_os2_console_test.exe: $(CONOBJS)
	link386 @console.lnk


clean:
	del $(OBJS) $(CONOBJS) *.map cmd32os2_os2.exe cmdos2_os2_console_test.exe