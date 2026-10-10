# Microsoft C/386 + IBM LINK386, with LIBC.LIB/OS2386.LIB on LIB.
CC = cl386
CFLAGS = /W3
viofetch.exe: viofetch.obj viofetch.def viofetch.lnk
	link386 @viofetch.lnk
viofetch.obj: VIOFETCH.C viofetch_api.h
	$(CC) $(CFLAGS) /c /Foviofetch.obj VIOFETCH.C
clean:
	del viofetch.obj viofetch.map viofetch.exe
