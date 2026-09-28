#
# 	Makefile 
# 
CC=cl386
TOUCH = touch
# Large memory model, register bug, optimize for time, remove stack probes:
CFLAGS = -DOS2 -DREGBUG -DLINT_ARGS -D_MSC_VER=0600

# All object modules
OBJS = decl.obj apply.obj bones.obj cmd.obj do.obj \
	do_name.obj do_wear.obj dog.obj eat.obj end.obj \
	engrave.obj fight.obj hack.obj invent.obj pcioctl.obj \
	lev.obj main.obj makemon.obj mhitu.obj mklev.obj \
	mkmaze.obj mkobj.obj mkshop.obj mon.obj \
	monst.obj o_init.obj objnam.obj options.obj \
	pager.obj potion.obj pri.obj \
	read.obj rip.obj rumors.obj save.obj \
	search.obj shk.obj shknam.obj steal.obj \
	termos2.obj timeout.obj topl.obj track.obj trap.obj \
	pctty.obj unix.obj u_init.obj vault.obj wield.obj \
	wizard.obj worm.obj worn.obj zap.obj \
	version.obj rnd.obj alloc.obj msdos.obj

hack.exe: onames.h $(OBJS)
	link386 @linkfile.o32

makedefs.exe: makedefs.c
	$(CC) $(CFLAGS) makedefs.c
#	link /NOE makedefs.obj,makedefs.exe,,, /stack:4000
#	del makedefs.map makedefs.obj

onames.h:	makedefs.exe objects.h
	makedefs > onames.h

clean:
	del hack.exe $(OBJS) makedefs.exe onames.h _makedefs.map _makedefs.exe makedefs.obj 
#	how to compile all the object files
#
$(OBJS) : $*.c
	$(CC) $(CFLAGS) /c $*.c

#	Other dependencies
#
Decl.obj :  onames.h hack.h mkroom.h
apply.obj :  hack.h edog.h mkroom.h
bones.obj :  hack.h
hack.obj :  hack.h
cmd.obj :  hack.h func_tab.h
do.obj :  hack.h
do_name.obj :  hack.h
do_wear.obj :  hack.h
dog.obj :  hack.h mfndpos.h edog.h mkroom.h
eat.obj :  hack.h
end.obj :  hack.h
engrave.obj :  hack.h
fight.obj :  hack.h
invent.obj :  hack.h wseg.h
pcioctl.obj :  config.h
lev.obj :  hack.h mkroom.h wseg.h
main.obj :  hack.h
makemon.obj :  hack.h
mhitu.obj :  hack.h
mklev.obj :  hack.h mkroom.h
mkmaze.obj :  hack.h mkroom.h
mkobj.obj :  hack.h
mkshop.obj :  hack.h mkroom.h eshk.h
mon.obj :  hack.h mfndpos.h
monst.obj :  hack.h eshk.h
o_init.obj :  config.h objects.h onames.h
objnam.obj :  hack.h
options.obj :  config.h hack.h
pager.obj :  hack.h
potion.obj :  hack.h
pri.obj :  hack.h wseg.h
read.obj :  hack.h
rip.obj :  hack.h
rumors.obj :  config.h
save.obj :  hack.h
search.obj :  hack.h
shk.obj :  hack.h mfndpos.h mkroom.h eshk.h
shknam.obj :  hack.h
steal.obj :  hack.h
termos2.obj :  config.h flag.h
timeout.obj :  hack.h
topl.obj :  hack.h
track.obj :  hack.h
trap.obj :  hack.h mkroom.h
pctty.obj :  hack.h
unix.obj :  hack.h mkroom.h
u_init.obj :  hack.h
vault.obj :  hack.h mkroom.h
version.obj :  date.h
wield.obj :  hack.h
wizard.obj :  hack.h
worm.obj :  hack.h wseg.h
worn.obj :  hack.h
zap.obj :  hack.h
msdos.obj : hack.h
termos2.obj : hack.h
#hack.h :  config.h objclass.h monst.h gold.h trap.h obj.h flag.h rm.h permonst.h onames.h
#	$(TOUCH) hack.h
#objects.h :  config.h objclass.h
#	$(TOUCH) objects.h
