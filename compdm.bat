mkdir DMDM19

nasm planar.asm -f obj

set GLOBOBJS=dmx.c tsmapi.c task_man.c i_main.c i_ibm.c i_sound.c planar.obj tables.c f_finale.c d_main.c d_net.c g_game.c m_menu.c m_misc.c am_map.c p_ceilng.c p_doors.c p_enemy.c p_floor.c p_inter.c p_lights.c p_map.c p_maputl.c p_plats.c p_pspr.c p_setup.c p_sight.c p_spec.c p_switch.c p_mobj.c p_telept.c p_tick.c p_user.c r_bsp.c r_data.c r_draw.c r_main.c r_plane.c r_segs.c r_things.c w_wad.c wi_stuff.c v_video.c st_lib.c st_stuff.c hu_stuff.c hu_lib.c s_sound.c z_zone.c info.c sounds.c dutils.c
dmc %GLOBOBJS% -mx X32.LIB -3 -o+all -DAPPVER_EXEDEF=DM19 -oDMDM19\dmdoom.exe

del *.obj
del dmdoom.map
