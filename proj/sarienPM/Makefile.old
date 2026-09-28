# Version 6.00.054	1989
# https://archive.org/details/os2ddk1.2
# PLATFORM = ddk12

# Version 6.00.054	1989
# https://archive.org/details/os2ddk2.0
# PLATFORM = ddk20

# Version 6.00.054	1989
# https://archive.org/details/os-2-cd-rom_202401
# PLATFORM = ddksrc

# Version 1.00.075	1989
# https://archive.org/details/msos2-200-sdk
# PLATFORM = c386

# Version 6.00.077	1991
# https://archive.org/details/windows-nt-3.1-build-196
# PLATFORM = nt-sep

# Version 6.00.080	1991
# https://archive.org/details/windows-nt-3.1-october-1991-build
# PLATFORM = nt-oct

# Version 6.00.081	1991
# https://archive.org/details/Windows_NT_and_Win32_Dev_Kit_1991
PLATFORM = nt-dec

# Version 8.00          1993
# PLATFORM = msvc32s

# Version 8.00.3200	1993
# Windows 95 73g SDk
# PLATFORM = 73g

# Version  13.10.4035	2002
# PLATFORM = v13
# PLATFORM = 13.10.6030

DEFS =  -D__32BIT__ -DM_I386 -D _MSC_VER=6 -DKEYS_WORK
INC = /u /w -Iinclude $(DEFS)
# OPT = /G3 /O
OPT = /G3 /Ogilt /Gs
DEBUG = #/Zi
LDEBUG = #-debug:full

# OS/2 compat dos extender (pharlap286)
DOSX = run286

# ms-dos emulator
EMU = msdos486

# dos exteded cross
CC =  $(EMU) $(DOSX) $(CL386ROOT)\$(PLATFORM)\cl386
# native CC
#CC =  $(CL386ROOT)\$(PLATFORM)\cl386

OS2LINK = $(EMU) $(DOSX) $(CL386ROOT)\ddk12\LINK386.EXE



OBJ = agi.obj agi_v2.obj agi_v3.obj agi_v4.obj checks.obj cli.obj console.obj cycle.obj \
 font.obj getopt.obj getopt1.obj global.obj graphics.obj hirespic.obj id.obj inv.obj \
keyboard.obj logic.obj lzw.obj main.obj menu.obj motion.obj objects.obj op_cmd.obj \
op_dbg.obj op_test.obj patches.obj path.obj picture.obj picview.obj rand.obj savegame.obj \
silent.obj sound.obj sprite.obj text.obj view.obj words.obj fileglob.obj nothing.obj

INVOLVED = fileglob.obj

HARD = dummy.obj pccga.obj pcvga.obj pharcga.obj pharvid.obj

NULL = dummy.obj nullvid.obj


# must include ONLY ONE strategey.. 
# for OS/2 it must have been assembled my MASM 6.11

include ..\-justcompile.mak
#include ..\-mangleassembly.mak
#include ..\-plainassembly.mak

sarien:  $(OBJ) $(NULL)
	SET LIB=C:\cl386-research\lib2
	$(OS2LINK) @sarien.lnk
	mcopy -i %QEMUPATH%\dummy.vfd -D o sarien.exe ::
	qemuos2
 

clean:
	del $(OBJ) $(NULL)
	del  sarien.exe
	del *.asm *.a1 *.a

