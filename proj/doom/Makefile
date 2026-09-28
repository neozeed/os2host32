INC = /u /w
OPT = /O
#DEBUG = /Zi
#LDEBUG = -debug:full

# native CC
CC =  cl386

OS2LINK = LINK386.EXE

PORTOBJ = am_map.obj   doomdef.obj doomstat.obj dstrings.obj \
d_items.obj d_main.obj d_net.obj fixdfsi.obj f_finale.obj f_wipe.obj g_game.obj \
hu_lib.obj hu_stuff.obj info.obj i_main.obj i_net.obj \
m_argv.obj m_bbox.obj m_cheat.obj m_fixed.obj m_menu.obj m_misc.obj m_random.obj \
m_swap.obj p_ceilng.obj p_doors.obj p_enemy.obj p_floor.obj p_inter.obj \
p_lights.obj p_map.obj p_maputl.obj p_mobj.obj p_plats.obj p_pspr.obj p_saveg.obj \
p_setup.obj p_sight.obj p_spec.obj p_switch.obj p_telept.obj p_tick.obj p_user.obj \
r_bsp.obj r_data.obj r_draw.obj r_main.obj r_plane.obj r_segs.obj r_sky.obj \
r_things.obj sounds.obj st_lib.obj st_stuff.obj \
tables.obj  v_video.obj wi_stuff.obj w_wad.obj z_zone.obj s_sound.obj

PLATOBJ = i_sound.obj i_video.obj i_system.obj

OBJ = $(PLATOBJ) $(PORTOBJ)

all: doom.exe

doom.exe: $(OBJ)
	link386 @doom.lnk

#m_fixed.obj:
#	ax386.exe m_fixed_i386.s -o m_fixed.obj

clean:
	del $(OBJ)
	del doom.exe doom2.exe
	del *.asm *.a1 *.a

