//
// Copyright (C) 2005-2014 Simon Howard
// Copyright (C) 2015-2017 Alexey Khokholov (Nuke.YKT)
// Copyright (C) 2023-2025 Frenkel Smeijers
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

#include "../id_heads.h"
#include "../dmx.h"

static void		*mus_data = NULL;
static uint8_t	*mid_data = NULL;

static int32_t mus_loop = 0;
static int32_t dmx_mus_port = 0;
static int32_t dmx_sdev = AHW_NONE;
static int32_t dmx_mdev = AHW_NONE;
static int32_t mus_rate = 140;
static int32_t mus_mastervolume = 127;

void MUS_PauseSong(int32_t handle)
{
	UNUSED(handle);
}

void MUS_ResumeSong(int32_t handle)
{
	UNUSED(handle);
}

void MUS_SetMasterVolume(int32_t volume)
{
	mus_mastervolume = volume;
}

int32_t MUS_RegisterSong(uint8_t *data)
{

	return 0;
}

void MUS_UnregisterSong(int32_t handle) {UNUSED(handle);}

void MUS_StopSong(int32_t handle)
{
	UNUSED(handle);
}

void MUS_ChainSong(int32_t handle, int32_t to)
{
	mus_loop = (to == handle);
}

void MUS_PlaySong(int32_t handle, int32_t volume)
{
}


int32_t SFX_PlayPatch(void *data, int32_t pitch, int32_t sep, int32_t volume, int32_t flags, int32_t priority)
{
	return -1;
}

void SFX_StopPatch(int32_t handle)
{
}

int32_t SFX_Playing(int32_t handle)
{
	return false;
}

void SFX_SetOrigin(int32_t handle, int32_t pitch, int32_t sep, int32_t volume)
{
}


int32_t ENS_Detect(void) {return -1;}
int32_t CODEC_Detect(int32_t *sbPort, int32_t *sbDma) {UNUSED(sbPort); UNUSED(sbDma); return -1;}
int32_t GF1_Detect(void) {return -1;}
void GF1_SetMap(uint8_t *dmxlump, int32_t size) {UNUSED(dmxlump); UNUSED(size);}


int32_t SB_Detect(int32_t *sbPort, int32_t *sbIrq, int32_t *sbDma, uint16_t *dspVersion)
{

	return 0;
}

void SB_SetCard(int32_t iBaseAddr, int32_t iIrq, int32_t iDma)
{
	UNUSED(iBaseAddr);
	UNUSED(iIrq);
	UNUSED(iDma);
}


int32_t AL_Detect(int32_t *wait, int32_t *type)
{
	UNUSED(wait);
	UNUSED(type);
	return -1;
}

void AL_SetCard(int32_t wait, void *data)
{
}


int32_t MPU_Detect(int32_t *mPort, int32_t *type)
{
	UNUSED(type);

	return -1;
}

void MPU_SetCard(int32_t mPort)
{
	dmx_mus_port = mPort;
}


int32_t DMX_Init(int32_t ticrate, int32_t maxsongs, uint32_t musicDevice, uint32_t sfxDevice)
{
	UNUSED(maxsongs);

	return musicDevice | sfxDevice;
}

void DMX_DeInit(void)
{
}


void WAV_PlayMode(int32_t channels, uint16_t sampleRate)
{
}
