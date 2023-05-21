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

#ifndef _DMX_H_
#define _DMX_H_

#include "tsmapi.h"

int32_t MUS_PauseSong(int32_t handle);
int32_t MUS_ResumeSong(int32_t handle);
void MUS_SetMasterVolume(int32_t volume);
int32_t MUS_RegisterSong(uint8_t *data);
int32_t MUS_UnregisterSong(int32_t handle);
int32_t MUS_StopSong(int32_t handle);
int32_t MUS_ChainSong(int32_t handle, int32_t to);
int32_t MUS_PlaySong(int32_t handle, int32_t volume);

int32_t SFX_PlayPatch(void *data, int32_t pitch, int32_t sep, int32_t volume, int32_t flags, int32_t priority);
void SFX_StopPatch(int32_t handle);
int32_t SFX_Playing(int32_t handle);
void SFX_SetOrigin(int32_t handle, int32_t pitch, int32_t sep, int32_t volume);

int32_t ENS_Detect(void);
int32_t CODEC_Detect(int32_t *sbPort, int32_t *sbDma);
int32_t GF1_Detect(void);
void GF1_SetMap(char *dmxlump, int32_t size);
int32_t SB_Detect(int32_t *sbPort, int32_t *sbIrq, int32_t *sbDma, uint16_t *version);
void SB_SetCard(int32_t iBaseAddr, int32_t iIrq, int32_t iDma);
int32_t AL_Detect(int32_t *wait, int32_t *type);
void AL_SetCard(int32_t wait, void *genmidi);
int32_t MPU_Detect(int32_t *mPort, int32_t *type);
void MPU_SetCard(int32_t mPort);

#define AHW_PC_SPEAKER		0x0001L
#define AHW_ADLIB			0x0002L
#define AHW_AWE32			0x0004L
#define AHW_SOUND_BLASTER	0x0008L
#define AHW_MPU_401			0x0010L
#define AHW_ULTRA_SOUND		0x0020L
#define AHW_MEDIA_VISION	0x0040L

#define AHW_ENSONIQ 		0x0100L
#define AHW_CODEC           0x0200L

int32_t DMX_Init(int32_t ticrate, int32_t maxsongs, uint32_t musicDevice, uint32_t sfxDevice);
void DMX_DeInit(void);

void WAV_PlayMode(int32_t channels, uint16_t sampleRate);
#endif
