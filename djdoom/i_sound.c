//
// Copyright (C) 1993-1996 Id Software, Inc.
// Copyright (C) 2023 Frenkel Smeijers
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

// I_SOUND.C

#include <stdio.h>
#include "doomdef.h"
#include "dmx.h"
#include "sounds.h"
#include "i_sound.h"

extern int _wp1, _wp2, _wp3, _wp4, _wp5, _wp6, _wp7, _wp8, _wp9, _wp10;
extern int _wp11, _wp12, _wp13, _wp14, _wp15, _wp16, _wp17, _wp18, _wp19;

/*
===============
=
= I_StartupTimer
=
===============
*/

static int tsm_ID;

void I_StartupTimer (void)
{
#ifndef NOTIMER
	extern void I_TimerISR(void);
	extern void I_InitBaseTime(void);

	printf("I_StartupTimer()\n");
	// installs master timer.  Must be done before StartupTimer()!
	TSM_Install(SND_TICRATE);
	tsm_ID = TSM_NewService (I_TimerISR, TICRATE); // max priority
	if (tsm_ID == -1)
	{
		I_Error("Can't register 35 Hz timer w/ DMX library");
	}

	I_InitBaseTime();
#endif
}

void I_ShutdownTimer (void)
{
	TSM_DelService(tsm_ID);
	TSM_Remove();
}

/*
 *
 *                           SOUND HEADER & DATA
 *
 *
 */

// sound information
#if 0
const char *dnames[] = {"None",
			"PC_Speaker",
			"Adlib",
			"Sound_Blaster",
			"ProAudio_Spectrum16",
			"Gravis_Ultrasound",
			"MPU",
			"AWE32"
			};
#endif

#if (APPVER_DOOMREV < AV_DR_DM18)
const char snd_prefixen[] = { 'P', 'P', 'A', 'S', 'S', 'S', 'M',
  'M', 'M', 'S'};
#else
const char snd_prefixen[] = { 'P', 'P', 'A', 'S', 'S', 'S', 'M',
  'M', 'M', 'S', 'S', 'S'};
#endif

int snd_DesiredMusicDevice, snd_DesiredSfxDevice;
int snd_MusicDevice,    // current music card # (index to dmxCodes)
	snd_SfxDevice,      // current sfx card # (index to dmxCodes)
	snd_MaxVolume,      // maximum volume for sound
	snd_MusicVolume;    // maximum volume for music
int dmxCodes[NUM_SCARDS]; // the dmx code for a given card

int     snd_SBport, snd_SBirq, snd_SBdma;       // sound blaster variables
int     snd_Mport;                              // midi variables

extern boolean  snd_MusicAvail, // whether music is available
		snd_SfxAvail;   // whether sfx are available

void I_PauseSong(int handle)
{
  MUS_PauseSong(handle);
}

void I_ResumeSong(int handle)
{
  MUS_ResumeSong(handle);
}

void I_SetMusicVolume(int volume)
{
  MUS_SetMasterVolume(volume);
  snd_MusicVolume = volume;
}

void I_SetSfxVolume(int volume)
{
  snd_MaxVolume = volume;
}

/*
 *
 *                              SONG API
 *
 */

int I_RegisterSong(void *data)
{
  return MUS_RegisterSong(data);
}

void I_UnRegisterSong(int handle)
{
  MUS_UnregisterSong(handle);
}

// Stops a song.  MUST be called before I_UnregisterSong().

void I_StopSong(int handle)
{
  MUS_StopSong(handle);

#if 0
  // Fucking kluge pause
  {
	int s;
	extern volatile int ticcount;
	for (s=ticcount ; ticcount - s < 10 ; );
  }
#endif
}

void I_PlaySong(int handle, boolean looping)
{
  MUS_ChainSong(handle, looping ? handle : -1);
  MUS_PlaySong(handle, snd_MusicVolume);
}

/*
 *
 *                                 SOUND FX API
 *
 */

// Gets lump nums of the named sound.  Returns pointer which will be
// passed to I_StartSound() when you want to start an SFX.  Must be
// sure to pass this to UngetSoundEffect() so that they can be
// freed!


int I_GetSfxLumpNum(sfxinfo_t *sound)
{
  char namebuf[9];

  if (sound->link) sound = sound->link;
  sprintf(namebuf, "d%c%s", snd_prefixen[snd_SfxDevice], sound->name);
  return W_GetNumForName(namebuf);

}

int I_StartSound (int id, void *data, int vol, int sep, int pitch, int priority)
{
  // hacks out certain PC sounds
  if (snd_SfxDevice == snd_PC
	&& (data == S_sfx[sfx_posact].data
	||  data == S_sfx[sfx_bgact].data
	||  data == S_sfx[sfx_dmact].data
	||  data == S_sfx[sfx_dmpain].data
	||  data == S_sfx[sfx_popain].data
	||  data == S_sfx[sfx_sawidl].data)) return -1;

  else
	return SFX_PlayPatch(data, sep, pitch, vol);

}

void I_StopSound(int handle)
{
//  extern volatile long gDmaCount;
//  long waittocount;
  SFX_StopPatch(handle);
//  waittocount = gDmaCount + 2;
//  while (gDmaCount < waittocount) ;
}

int I_SoundIsPlaying(int handle)
{
  return SFX_Playing(handle);
}

void I_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
  SFX_SetOrigin(handle, pitch, sep, vol);
}

/*
 *
 *                                                      SOUND STARTUP STUFF
 *
 *
 */

//
// Why PC's Suck, Reason #8712
//

void I_sndArbitrateCards(void)
{
  // boolean gus, adlib, pc, sb, midi, ensoniq, codec;
#if (APPVER_DOOMREV < AV_DR_DM18)
  boolean gus, adlib, pc, sb, midi;
#else
  boolean codec, ensoniq, gus, adlib, pc, sb, midi;
#endif
  int wait, dmxlump;

  snd_MaxVolume = 127;

  snd_MusicDevice = snd_DesiredMusicDevice;
  snd_SfxDevice = snd_DesiredSfxDevice;

  // check command-line parameters- overrides config file
  //
  if (M_CheckParm("-nosound")) snd_MusicDevice = snd_SfxDevice = snd_none;
  if (M_CheckParm("-nosfx")) snd_SfxDevice = snd_none;
  if (M_CheckParm("-nomusic")) snd_MusicDevice = snd_none;

  if (snd_MusicDevice > snd_MPU && snd_MusicDevice <= snd_MPU3)
	snd_MusicDevice = snd_MPU;
  if (snd_MusicDevice == snd_SB)
	snd_MusicDevice = snd_Adlib;
  if (snd_MusicDevice == snd_PAS)
	snd_MusicDevice = snd_Adlib;

  // figure out what i've got to initialize
  //
  gus = snd_MusicDevice == snd_GUS || snd_SfxDevice == snd_GUS;
  sb = snd_SfxDevice == snd_SB || snd_MusicDevice == snd_SB;
#if (APPVER_DOOMREV >= AV_DR_DM18)
  ensoniq = snd_SfxDevice == snd_ENS ;
  codec = snd_SfxDevice == snd_CODEC ;
#endif
  adlib = snd_MusicDevice == snd_Adlib ;
  pc = snd_SfxDevice == snd_PC;
  midi = snd_MusicDevice == snd_MPU;

#if (APPVER_DOOMREV >= AV_DR_DM18)
  // initialize whatever i've got
  //
  if (ensoniq)
  {
	if (devparm)
	  printf("ENSONIQ\n");
	if (ENS_Detect())
	  printf("Dude.  The ENSONIQ ain't responding.\n");
  }
  if (codec)
  {
	if (devparm)
	  printf("CODEC p=0x%x, d=%d\n", snd_SBport, snd_SBdma);
	if (CODEC_Detect(&snd_SBport, &snd_SBdma))
	  printf("CODEC.  The CODEC ain't responding.\n");
  }
#endif
  if (gus)
  {
	if (devparm)
	  printf("GUS\n");
	fprintf(stderr, "GUS1\n");
	if (GF1_Detect()) printf("Dude.  The GUS ain't responding.\n");
	else
	{
	  fprintf(stderr, "GUS2\n");
	  if (commercial)
	    dmxlump = W_GetNumForName("dmxgusc");
	  else
	    dmxlump = W_GetNumForName("dmxgus");
	  GF1_SetMap(W_CacheLumpNum(dmxlump, PU_CACHE), lumpinfo[dmxlump].size);
	}

  }
  if (sb)
  {
	if(devparm)
	{
	  printf("cfg p=0x%x, i=%d, d=%d\n",
	  snd_SBport, snd_SBirq, snd_SBdma);
	}
	if (SB_Detect(&snd_SBport, &snd_SBirq, &snd_SBdma))
	{
	  printf("SB isn't responding at p=0x%x, i=%d, d=%d\n",
	  snd_SBport, snd_SBirq, snd_SBdma);
	}
	else SB_SetCard(snd_SBport, snd_SBirq, snd_SBdma);

	if(devparm)
	{
	  printf("SB_Detect returned p=0x%x,i=%d,d=%d\n",
	  snd_SBport, snd_SBirq, snd_SBdma);
	}
  }

  if (adlib)
  {
	if(devparm)
	  printf("Adlib\n");
	if (AL_Detect(&wait))
	  printf("Dude.  The Adlib isn't responding.\n");
	else
		AL_SetCard(wait, W_CacheLumpName("genmidi", PU_STATIC));
  }

  if (midi)
  {
	if (devparm)
	  printf("Midi\n");
	if (devparm)
	  printf("cfg p=0x%x\n", snd_Mport);

	if (MPU_Detect(&snd_Mport))
	  printf("The MPU-401 isn't reponding @ p=0x%x.\n", snd_Mport);
	else MPU_SetCard(snd_Mport);
  }

}

// inits all sound stuff

void I_StartupSound (void)
{
  int rc, i;

  if (devparm)
	printf("I_StartupSound: Hope you hear a pop.\n");

  // initialize dmxCodes[]
  dmxCodes[0] = 0;
  dmxCodes[snd_PC] = AHW_PC_SPEAKER;
  dmxCodes[snd_Adlib] = AHW_ADLIB;
  dmxCodes[snd_SB] = AHW_SOUND_BLASTER;
  dmxCodes[snd_PAS] = AHW_MEDIA_VISION;
  dmxCodes[snd_GUS] = AHW_ULTRA_SOUND;
  dmxCodes[snd_MPU] = AHW_MPU_401;
  dmxCodes[snd_AWE] = AHW_AWE32;
#if (APPVER_DOOMREV >= AV_DR_DM18)
  dmxCodes[snd_ENS] = AHW_ENSONIQ;
  dmxCodes[snd_CODEC] = AHW_CODEC;
#endif

  // inits sound library timer stuff
  I_StartupTimer();

  // pick the sound cards i'm going to use
  //
  I_sndArbitrateCards();

  if (devparm)
  {
	printf("  Music device #%d & dmxCode=%d\n", snd_MusicDevice,
	  dmxCodes[snd_MusicDevice]);
	printf("  Sfx device #%d & dmxCode=%d\n", snd_SfxDevice,
	  dmxCodes[snd_SfxDevice]);
  }

  // inits DMX sound library
  printf("  calling DMX_Init\n");
  rc = DMX_Init(SND_TICRATE, SND_MAXSONGS, dmxCodes[snd_MusicDevice],
	dmxCodes[snd_SfxDevice]);

  if (devparm)
	printf("  DMX_Init() returned %d\n", rc);

}

// shuts down all sound stuff

void I_ShutdownSound (void)
{
  S_PauseSound();
#if 0
  {
	int s;
	extern volatile int ticcount;
	for (s=ticcount + 30; s != ticcount ; );
  }
#endif
  DMX_DeInit();
}

void I_SetChannels(int channels)
{
  WAV_PlayMode(channels, SND_SAMPLERATE);
}
