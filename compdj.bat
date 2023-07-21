if "%DJDIR%" == "" goto error

mkdir DJDM19

nasm a_mv_mix.asm -f coff
nasm planar.asm -f coff

set CFLAGS=-Ofast -march=i386 -flto -fwhole-program -fomit-frame-pointer -funroll-loops -fgcse-sm -fgcse-las -fipa-pta -mpreferred-stack-boundary=2 -Wno-attributes -Wpedantic
@rem set CFLAGS=%CFLAGS% -Wall -Wextra

set GLOBOBJS=dmx.c a_al_mid.c a_blast.c a_dma.c a_ll_man.c a_midi.c a_mpu401.c a_multiv.c a_music.c a_musmid.c a_mv_mix.o a_pcfx.c a_taskmn.c a_tsmapi.c i_main.c i_ibm.c i_sound.c planar.o tables.c f_finale.c d_main.c d_net.c g_game.c m_menu.c m_misc.c am_map.c p_ceilng.c p_doors.c p_enemy.c p_floor.c p_inter.c p_lights.c p_map.c p_maputl.c p_plats.c p_pspr.c p_setup.c p_sight.c p_spec.c p_switch.c p_mobj.c p_telept.c p_tick.c p_user.c r_bsp.c r_data.c r_draw.c r_main.c r_plane.c r_segs.c r_things.c w_wad.c wi_stuff.c v_video.c st_lib.c st_stuff.c hu_stuff.c hu_lib.c s_sound.c z_zone.c info.c sounds.c dutils.c
gcc -DAPPVER_EXEDEF=DM19 %GLOBOBJS% %CFLAGS% -o DJDM19/djdoom.exe
strip -s DJDM19/djdoom.exe
stubedit DJDM19/djdoom.exe dpmi=CWSDPR0.EXE

del a_mv_mix.o
del planar.o

goto end

:error
@echo Set the environment variables before running this script!

:end
