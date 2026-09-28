
DEFS =  -D__32BIT__ -DM_I386 -D _MSC_VER=6 -DKEYS_WORK -DNEED_NOTHING
INC = -Iinclude
#OPT = /G3 /O
#OPT = /G3 /Ogilt /Gs
DEBUG = #/Zi
LDEBUG = #-debug:full
CC=gcc
OS2LINK = LINK386.EXE

CFLAGS= $(INC) $(DEFS) $(OPT)

OBJ = agi.obj agi_v2.obj agi_v3.obj agi_v4.obj checks.obj cli.obj console.obj cycle.obj \
 font.obj getopt.obj getopt1.obj global.obj graphics.obj hirespic.obj id.obj inv.obj \
keyboard.obj logic.obj lzw.obj main.obj menu.obj motion.obj objects.obj op_cmd.obj \
op_dbg.obj op_test.obj patches.obj path.obj picture.obj picview.obj rand.obj savegame.obj \
silent.obj sound.obj sprite.obj text.obj view.obj words.obj nothing.obj

sarien:  $(OBJ) 

# gnu make (cross compiled)
%.obj: %.c
	$(CC) $(CFLAGS) -c $*.c
	o2obj $*.o
	rm $*.o
# nmake
#.c.obj:
#	@emxomf -o $*.obj $*.o
#	@del $*.o


clean:
	del $(OBJ)
	del  sarien.exe
	del *.asm *.a1 *.a
