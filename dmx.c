//
// Copyright (C) 2005-2014 Simon Howard
// Copyright (C) 2015-2017 Alexey Khokholov (Nuke.YKT)
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

#include "doomdef.h"
#include "dmx.h"
#include "pcfx.h"

typedef enum
{
	SoundBlaster,
	ProAudioSpectrum,
	SoundMan16,
	Adlib,
	GenMidi,
	SoundCanvas,
	Awe32,
	WaveBlaster,
	SoundScape,
	UltraSound,
	SoundSource,
	TandySoundSource,
	PC,
	NumSoundCards
} soundcardnames;

enum MUSIC_ERRORS
{
	MUSIC_Warning = -2,
	MUSIC_Error   = -1,
	MUSIC_Ok      = 0,
	MUSIC_ASSVersion,
	MUSIC_SoundCardError,
	MUSIC_MPU401Error,
	MUSIC_InvalidCard,
	MUSIC_MidiError,
	MUSIC_TaskManError,
	MUSIC_FMNotDetected,
	MUSIC_DPMI_Error
};
   
static void		*mus_data = NULL;
static uint8_t	*mid_data = NULL;

static int32_t mus_loop = 0;
static int32_t dmx_mdev = 0;
static int32_t mus_rate = 140;
static int32_t mus_mastervolume = 127;

void MUS_PauseSong(int32_t handle)
{
	UNUSED(handle);
	MUSIC_Pause();
}

void MUS_ResumeSong(int32_t handle)
{
	UNUSED(handle);
	MUSIC_Continue();
}

void MUS_SetMasterVolume(int32_t volume)
{
	mus_mastervolume = volume;
	MUSIC_SetVolume(volume * 2);
}

int32_t MUS_RegisterSong(uint8_t *data)
{
	FILE *mus;
	FILE *mid;
	uint32_t midlen;
	uint16_t len;
	mus_data = NULL;
	len = ((uint16_t*)data)[2] + ((uint16_t*)data)[3];
	if (mid_data)
		free(mid_data);

	if (memcmp(data, "MThd", 4))
	{
		mus = fopen("temp.mus", "wb");
		if (!mus)
			return 0;

		fwrite(data, 1, len, mus);
		fclose(mus);
		mus = fopen("temp.mus", "rb");
		if (!mus)
			return 0;

		mid = fopen("temp.mid", "wb");
		if (!mid)
		{
			fclose(mus);
			return 0;
		}
		if (mus2mid(mus, mid, mus_rate, dmx_mdev == Adlib || dmx_mdev == SoundBlaster))
		{
			fclose(mid);
			fclose(mus);
			return 0;
		}
		fclose(mid);
		fclose(mus);
		mid = fopen("temp.mid", "rb");
		if (!mid)
			return 0;

		fseek(mid, 0, SEEK_END);
		midlen = ftell(mid);
		rewind(mid);
		mid_data = malloc(midlen);
		if (!mid_data)
		{
			fclose(mid);
			return 0;
		}
		fread(mid_data, 1, midlen, mid);
		fclose(mid);
		mus_data = mid_data;
		remove("temp.mid");
		remove("temp.mus");
		return 0;
	} else {
		mus_data = data;
		return 0;
	}
}

void MUS_UnregisterSong(int32_t handle) {UNUSED(handle);}

void MUS_StopSong(int32_t handle)
{
	UNUSED(handle);
	MUSIC_StopSong();
}

void MUS_ChainSong(int32_t handle, int32_t to)
{
	mus_loop = (to == handle);
}

void MUS_PlaySong(int32_t handle, int32_t volume)
{
	int32_t status;

	UNUSED(handle);
	UNUSED(volume);

	if (mus_data == NULL)
		return;

	status = MUSIC_PlaySong((uint8_t*)mus_data, mus_loop);
	if (status == MUSIC_Ok)
		MUSIC_SetVolume(mus_mastervolume * 2);
}


int32_t SFX_PlayPatch(void *vdata, int32_t pitch, int32_t sep, int32_t volume, int32_t flags, int32_t priority)
{
	uint16_t type = ((uint16_t*)vdata)[0];

	UNUSED(pitch);
	UNUSED(sep);
	UNUSED(volume);
	UNUSED(flags);
	UNUSED(priority);

	if (type == 0)
	{
		uint32_t pcshandle = PCFX_Play(vdata);
		return pcshandle | 0x8000;
	}
	else
		return -1;
}

void SFX_StopPatch(int32_t handle)
{
	if (handle & 0x8000)
		PCFX_Stop(handle & 0x7fff);
}

int32_t SFX_Playing(int32_t handle)
{
	if (handle & 0x8000)
		return PCFX_SoundPlaying(handle & 0x7fff);
	else
		return 0;
}

void SFX_SetOrigin(int32_t handle, int32_t pitch, int32_t sep, int32_t volume) {UNUSED(handle); UNUSED(pitch); UNUSED(sep); UNUSED(volume);}


int32_t ENS_Detect(void) {return -1;}
int32_t CODEC_Detect(int32_t *sbPort, int32_t *sbDma) {UNUSED(sbPort); UNUSED(sbDma); return -1;}
int32_t GF1_Detect(void) {return -1;}
void GF1_SetMap(char *dmxlump, int32_t size) {UNUSED(dmxlump); UNUSED(size);}
int32_t SB_Detect(int32_t *sbPort, int32_t *sbIrq, int32_t *sbDma, uint16_t *version) {UNUSED(sbPort); UNUSED(sbIrq); UNUSED(sbDma); UNUSED(version); return -1;}
void SB_SetCard(int32_t iBaseAddr, int32_t iIrq, int32_t iDma) {UNUSED(iBaseAddr); UNUSED(iIrq); UNUSED(iDma);}
int32_t AL_Detect(int32_t *wait, int32_t *type) {UNUSED(wait); UNUSED(type); return -1;}
void AL_SetCard(int32_t wait, void *genmidi) {UNUSED(wait); UNUSED(genmidi);}
int32_t MPU_Detect(int32_t *mPort, int32_t *type) {UNUSED(mPort); UNUSED(type); return -1;}
void MPU_SetCard(int32_t mPort) {UNUSED(mPort);}


int32_t DMX_Init(int32_t ticrate, int32_t maxsongs, uint32_t musicDevice, uint32_t sfxDevice)
{
	UNUSED(maxsongs);
	UNUSED(musicDevice);

	if (sfxDevice & AHW_PC_SPEAKER)
		PCFX_Init(ticrate);

	return sfxDevice;
}

void DMX_DeInit(void)
{
	PCFX_Shutdown();
}


void WAV_PlayMode(int32_t channels, uint16_t sampleRate) {UNUSED(channels); UNUSED(sampleRate);}
