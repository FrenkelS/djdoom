//
//
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

static unsigned short divisors[] = {
	0,
	6818, 6628, 6449, 6279, 6087, 5906, 5736, 5575,
	5423, 5279, 5120, 4971, 4830, 4697, 4554, 4435,
	4307, 4186, 4058, 3950, 3836, 3728, 3615, 3519,
	3418, 3323, 3224, 3131, 3043, 2960, 2875, 2794,
	2711, 2633, 2560, 2485, 2415, 2348, 2281, 2213,
	2153, 2089, 2032, 1975, 1918, 1864, 1810, 1757,
	1709, 1659, 1612, 1565, 1521, 1478, 1435, 1395,
	1355, 1316, 1280, 1242, 1207, 1173, 1140, 1107,
	1075, 1045, 1015,  986,  959,  931,  905,  879,
	 854,  829,  806,  783,  760,  739,  718,  697,
	 677,  658,  640,  621,  604,  586,  570,  553,
	 538,  522,  507,  493,  479,  465,  452,  439,
	 427,  415,  403,  391,  380,  369,  359,  348,
	 339,  329,  319,  310,  302,  293,  285,  276,
	 269,  261,  253,  246,  239,  232,  226,  219,
	 213,  207,  201,  195,  190,  184,  179,
};

typedef struct {
	unsigned int length;
	unsigned short priority;
	unsigned short data[0x10000];
} pcspkmuse_t;

typedef struct {
	unsigned short id;
	unsigned short length;
	unsigned char data[];
} dmxpcs_t;

static pcspkmuse_t pcspkmuse;
static int pcshandle = 0;

int32_t MUS_PauseSong(int32_t handle) {UNUSED(handle); return 0;}
int32_t MUS_ResumeSong(int32_t handle) {UNUSED(handle); return 0;}
void MUS_SetMasterVolume(int32_t volume) {UNUSED(volume);}
int32_t MUS_RegisterSong(uint8_t *data) {UNUSED(data); return 0;}
int32_t MUS_UnregisterSong(int32_t handle) {UNUSED(handle); return 0;}
int32_t MUS_StopSong(int32_t handle) {UNUSED(handle); return 0;}
int32_t MUS_ChainSong(int32_t handle, int32_t to) {UNUSED(handle); UNUSED(to); return 0;}
int32_t MUS_PlaySong(int32_t handle, int32_t volume) {UNUSED(handle); UNUSED(volume); return 0;}

int32_t SFX_PlayPatch(void *vdata, int32_t pitch, int32_t sep, int32_t volume, int32_t flags, int32_t priority)
{
	unsigned int rate;
	unsigned long len;
	unsigned char *data = (unsigned char*)vdata;
	unsigned int type = (data[1] << 8) | data[0];
	dmxpcs_t *dmxpcs = (dmxpcs_t*)vdata;
	unsigned short i;

	UNUSED(flags);

	if (type == 0)
	{
		pcspkmuse.length = dmxpcs->length * 2;
		pcspkmuse.priority = 100;
		for (i = 0; i < dmxpcs->length; i++)
		{
			pcspkmuse.data[i] = divisors[dmxpcs->data[i]];
		}
		pcshandle = PCFX_Play((PCSound *)&pcspkmuse, 100, 0);
		return pcshandle | 0x8000;
	}
	else if (type == 3)
	{
		rate = (data[3] << 8) | data[2];
		len = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];
		if (len <= 48) {
			return -1;
		}
		len -= 32;
		return FX_PlayRaw(data + 24, len, rate, ((pitch - 128) * 2400) / 128, volume * 2, ((254 - sep) * volume) / 63, ((sep)* volume) / 63, 127-priority, 0);
	}
	else
		return 0;
}

void SFX_StopPatch(int32_t handle)
{
	if (handle & 0x8000)
		PCFX_Stop(handle & 0x7fff);
	else
		FX_StopSound(handle);
}

int32_t SFX_Playing(int32_t handle)
{
	if (handle & 0x8000)
		return PCFX_SoundPlaying(handle & 0x7fff);
	else
		return FX_SoundActive(handle);
}

void SFX_SetOrigin(int32_t handle, int32_t pitch, int32_t sep, int32_t volume)
{
	if (!(handle & 0x8000))
	{
		FX_SetPan(handle, volume * 2, ((254 - sep) * volume) / 63, ((sep)* volume) / 63);
		FX_SetPitch(handle, ((pitch - 128) * 2400) / 128);
	}
}


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
	long status, device;
	mus_rate = ticrate;
	dmx_sdev = sfxDevice;
	status = 0;

	UNUSED(maxsongs);

	switch (musicDevice) {
	case 0:
		device = NumSoundCards;
		break;
	case AHW_ADLIB:
		if (sfxDevice & AHW_SOUND_BLASTER)
			device = SoundBlaster;
		else
			device = Adlib;
		break;
	case AHW_SOUND_BLASTER:
		device = SoundBlaster;
		break;
	case AHW_MPU_401:
		device = GenMidi;
		break;
	case AHW_ULTRA_SOUND:
		device = UltraSound;
		break;
	default:
		return -1;
		break;
	}
	dmx_mdev = device;
	if (device == SoundBlaster)
	{
		int MaxVoices;
		int MaxBits;
		int MaxChannels;

		FX_SetupSoundBlaster(dmx_blaster, (int *)&MaxVoices, (int *)&MaxBits, (int *)&MaxChannels);
	}
	status = MUSIC_Init(device, dmx_mus_port);
	if (status == MUSIC_Ok) {
		MUSIC_SetVolume(0);
	}
	if (sfxDevice & AHW_PC_SPEAKER)
	{
		PCFX_Init();
		PCFX_SetTotalVolume(255);
		PCFX_UseLookup(0, 0);
	}
	return musicDevice | sfxDevice;
}

void DMX_DeInit(void)
{
	MUSIC_Shutdown();
	FX_Shutdown();
	PCFX_Shutdown();
	remove("ULTRAMID.INI");
	if (mid_data)
	{
		free(mid_data);
	}
}


void WAV_PlayMode(int32_t channels, uint16_t sampleRate) {UNUSED(channels); UNUSED(sampleRate);}
