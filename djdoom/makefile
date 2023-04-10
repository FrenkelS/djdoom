
# NEWDOOM.EXE and DOOM.EXE makefile

# --------------------------------------------------------------------------
#
#      4r  use 80486 timings and register argument passing
#       c  compile only
#      d1  include line number debugging information
#      d2  include full sybolic debugging information
#      ei  force enums to be of type int
#       j  change char default from unsigned to signed
#      oa  relax aliasing checking
#      od  do not optimize
#  oe[=#]  expand functions inline, # = quads (default 20)
#      oi  use the inline library functions
#      om  generate inline 80x87 code for math functions
#      ot  optimize for time
#      ox  maximum optimization
#       s  remove stack overflow checks
#     zp1  align structures on bytes
#      zq  use quiet mode
#  /i=dir  add include directories
#
# --------------------------------------------------------------------------

CCOPTS = /dAPPVER_EXEDEF=$(appver_exedef) /omaxet /zp1 /4r /ei /j /zq

!ifeq appver_exedef DM18FR
GAMEVEROPTS = /DFRENCH
!endif

!ifeq appver_exedef DM1666P
DMXVER=dmx34af1
!else ifeq appver_exedef DM1666E
DMXVER=dmx34af1
!else ifeq appver_exedef DM1666
DMXVER=dmx34af2
!else ifeq appver_exedef DM17
DMXVER=dmx34af3
!else ifeq appver_exedef DM17A
DMXVER=dmx34af3
!else ifeq appver_exedef DM18FR
DMXVER=dmx34af3
!else
DMXVER=dmx37
!endif

!ifeq use_apodmx 1
DMXINC = /i=..\apodmx
DMXLIBS = file ..\..\apodmx\apodmx.lib file audio_wf.lib
!else ifeq DMXVER dmx37
DMXINC = /i=..\dmx\$(DMXVER)\inc
DMXLIBS = file ..\..\dmx\$(DMXVER)\lib\dmx_r.lib
!else
DMXINC = /i=..\dmx\$(DMXVER)\inc
DMXLIBS = file ..\..\dmx\$(DMXVER)\lib\dmx.lib
!endif

GLOBOBJS = &
 i_main.obj &
 i_ibm.obj &
 i_ibm_a.obj &
 i_sound.obj &
 i_cyber.obj &
 planar.obj &
 tables.obj &
 f_finale.obj &
 d_main.obj &
 d_net.obj &
 g_game.obj &
 m_menu.obj &
 m_misc.obj &
 am_map.obj &
 p_ceilng.obj &
 p_doors.obj &
 p_enemy.obj &
 p_floor.obj &
 p_inter.obj &
 p_lights.obj &
 p_map.obj &
 p_maputl.obj &
 p_plats.obj &
 p_pspr.obj &
 p_setup.obj &
 p_sight.obj &
 p_spec.obj &
 p_switch.obj &
 p_mobj.obj &
 p_telept.obj &
 p_tick.obj &
 p_user.obj &
 r_bsp.obj &
 r_data.obj &
 r_draw.obj &
 r_main.obj &
 r_plane.obj &
 r_segs.obj &
 r_things.obj &
 w_wad.obj &
 wi_stuff.obj &
 v_video.obj &
 st_lib.obj &
 st_stuff.obj &
 hu_stuff.obj &
 hu_lib.obj &
 s_sound.obj &
 z_zone.obj &
 info.obj &
 sounds.obj &
 dutils.obj

$(appver_exedef)\newdoom.exe : $(GLOBOBJS)
 cd $(appver_exedef)
 # Workaround for too long path
!ifeq use_apodmx 1
 copy ..\..\audiolib\origlibs\109\AUDIO_WF.LIB .
!endif
 call ..\linkhlpr.bat $(DMXLIBS)
 copy newdoom.exe strpdoom.exe
 wstrip strpdoom.exe
 cd..

.obj : $(appver_exedef)

.c.obj :
 wcc386p $(CCOPTS) $(GAMEVEROPTS) $(DMXINC) $[* /fo=$(appver_exedef)\$^&

.asm.obj :
 tasm /mx $[*,$(appver_exedef)\$^&/J

clean : .SYMBOLIC
 del $(appver_exedef)\*.obj
 del $(appver_exedef)\newdoom.exe
