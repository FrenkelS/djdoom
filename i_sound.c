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

#include "doomdef.h"
#include "dmx.h"
#include "sounds.h"

#define SND_TICRATE     140     // tic rate for updating sound
#define SND_MAXSONGS    40      // max number of songs in game
#define SND_SAMPLERATE  11025   // sample rate of sound effects

/*
===============
=
= I_StartupTimer
=
===============
*/

static int32_t tsm_ID;

static void I_StartupTimer (void)
{
	extern void I_InitBaseTime(void);
	extern void I_TimerISR(void);

	printf("I_StartupTimer()\n");

#if defined NOTIMER
	I_InitBaseTime();
#else
	// installs master timer.  Must be done before StartupTimer()!
	TSM_Install(SND_TICRATE);
	tsm_ID = TSM_NewService (I_TimerISR, TICRATE, 0, 0); // max priority
	if (tsm_ID == -1)
	{
		I_Error("Can't register 35 Hz timer w/ DMX library");
	}
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
#if (APPVER_DOOMREV < AV_DR_DM18)
static const char snd_prefixen[] = { 'P', 'P', 'A', 'S', 'S', 'S', 'M',
  'M', 'M', 'S'};
#else
static const char snd_prefixen[] = { 'P', 'P', 'A', 'S', 'S', 'S', 'M',
  'M', 'M', 'S', 'S', 'S'};
#endif

typedef enum
{
  snd_none,		// NO MUSIC / NO SOUND FX
  snd_PC,		// PC Speaker
  snd_Adlib,	// Adlib
  snd_SB,		// Sound Blaster
  snd_PAS,		// Pro Audio Spectrum
  snd_GUS,		// Gravis UltraSound
  snd_MPU,		// WaveBlaster
  snd_MPU2,		// Sound Canvas
  snd_MPU3,		// General MIDI
  snd_AWE,		// Sound Blaster AWE32
#if (APPVER_DOOMREV >= AV_DR_DM18)
  snd_ENS,
  snd_CODEC,
#endif
  NUM_SCARDS
} cardenum_t;

int32_t snd_DesiredMusicDevice, snd_DesiredSfxDevice;
static int32_t snd_MusicDevice;    // current music card # (index to dmxCodes)
static int32_t snd_SfxDevice;      // current sfx card # (index to dmxCodes)
static int32_t snd_MusicVolume;    // maximum volume for music
static int32_t dmxCodes[NUM_SCARDS]; // the dmx code for a given card

int32_t     snd_SBport, snd_SBirq, snd_SBdma;       // sound blaster variables
int32_t     snd_Mport;                              // midi variables

void I_PauseSong(int32_t handle)
{
  MUS_PauseSong(handle);
}

void I_ResumeSong(int32_t handle)
{
  MUS_ResumeSong(handle);
}

void I_SetMusicVolume(int32_t volume)
{
  MUS_SetMasterVolume(volume);
  snd_MusicVolume = volume;
}

/*
 *
 *                              SONG API
 *
 */

int32_t I_RegisterSong(void *data)
{
  return MUS_RegisterSong(data);
}

void I_UnRegisterSong(int32_t handle)
{
  MUS_UnregisterSong(handle);
}

// Stops a song.  MUST be called before I_UnregisterSong().

void I_StopSong(int32_t handle)
{
  MUS_StopSong(handle);

  // Fucking kluge pause
  {
	int32_t s = I_GetTime();
	while (I_GetTime() - s < 10)
	{
	}
  }
}

void I_PlaySong(int32_t handle, boolean looping)
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


int32_t I_GetSfxLumpNum(sfxinfo_t *sound)
{
  char namebuf[9];

  if (sound->link) sound = sound->link;
  sprintf(namebuf, "d%c%s", snd_prefixen[snd_SfxDevice], sound->name);
  return W_GetNumForName(namebuf);

}

int32_t I_StartSound (void *data, int32_t vol, int32_t sep, int32_t pitch)
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
	return SFX_PlayPatch(data, sep, pitch, vol, 0, 100);

}

void I_StopSound(int32_t handle)
{
  SFX_StopPatch(handle);
}

boolean I_SoundIsPlaying(int32_t handle)
{
  return SFX_Playing(handle) != 0;
}

void I_UpdateSoundParams(int32_t handle, int32_t vol, int32_t sep, int32_t pitch)
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

static void I_sndArbitrateCards(void)
{
  // boolean gus, adlib, pc, sb, midi, ensoniq, codec;
#if (APPVER_DOOMREV < AV_DR_DM18)
  boolean gus, adlib, sb, midi;
#else
  boolean codec, ensoniq, gus, adlib, sb, midi;
#endif
  int32_t cardType, wait, dmxlump;

  snd_MusicDevice = snd_DesiredMusicDevice;
  snd_SfxDevice = snd_DesiredSfxDevice;

  // check command-line parameters- overrides config file
  //
  if (M_CheckParm("-nosound")) snd_MusicDevice = snd_SfxDevice = snd_none;
  if (M_CheckParm("-nosfx")) snd_SfxDevice = snd_none;
  if (M_CheckParm("-nomusic")) snd_MusicDevice = snd_none;

  if (snd_MusicDevice == snd_MPU2 || snd_MusicDevice == snd_MPU3)
	snd_MusicDevice = snd_MPU;
  else if (snd_MusicDevice == snd_SB || snd_MusicDevice == snd_PAS)
	snd_MusicDevice = snd_Adlib;

  // figure out what i've got to initialize
  //
  gus = snd_MusicDevice == snd_GUS || snd_SfxDevice == snd_GUS;
  sb = snd_SfxDevice == snd_SB;
#if (APPVER_DOOMREV >= AV_DR_DM18)
  ensoniq = snd_SfxDevice == snd_ENS ;
  codec = snd_SfxDevice == snd_CODEC ;
#endif
  adlib = snd_MusicDevice == snd_Adlib ;
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
	if (SB_Detect(&snd_SBport, &snd_SBirq, &snd_SBdma, 0))
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
	if (AL_Detect(&wait,0))
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

	if (MPU_Detect(&snd_Mport, &cardType))
	  printf("The MPU-401 isn't reponding @ p=0x%x.\n", snd_Mport);
	else MPU_SetCard(snd_Mport);
  }

}

boolean I_IsAdlib(void)
{
	return snd_MusicDevice == snd_Adlib;
}

// inits all sound stuff

void I_StartupSound (void)
{
  int32_t rc;

  if (devparm)
	printf("I_StartupSound: Hope you hear a pop.\n");

  // initialize dmxCodes[]
  dmxCodes[snd_none]  = AHW_NONE;
  dmxCodes[snd_PC]    = AHW_PC_SPEAKER;
  dmxCodes[snd_Adlib] = AHW_ADLIB;
  dmxCodes[snd_SB]    = AHW_SOUND_BLASTER;
  dmxCodes[snd_PAS]   = AHW_MEDIA_VISION;
  dmxCodes[snd_GUS]   = AHW_ULTRA_SOUND;
  dmxCodes[snd_MPU]   = AHW_MPU_401;
  dmxCodes[snd_AWE]   = AHW_AWE32;
#if (APPVER_DOOMREV >= AV_DR_DM18)
  dmxCodes[snd_ENS]   = AHW_ENSONIQ;
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

  {
	int32_t s = I_GetTime();
	while (I_GetTime() - s < 30)
	{
	}
  }

  DMX_DeInit();
}

void I_SetChannels(int32_t channels)
{
  WAV_PlayMode(channels, SND_SAMPLERATE);
}
