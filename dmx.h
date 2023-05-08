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

#ifndef __DMX__
#define __DMX__

int TSM_Install(unsigned int sndTicrate);
int TSM_NewService(int (*timerISR)(void), int frequency, int priority, int pause);
void TSM_DelService(int tsm_ID);
void TSM_Remove(void);

int MUS_PauseSong(int handle);
int MUS_ResumeSong(int handle);
void MUS_SetMasterVolume(int volume);
int MUS_RegisterSong(unsigned char *data);
int MUS_UnregisterSong(int handle);
int MUS_StopSong(int handle);
int MUS_ChainSong(int handle, int to);
int MUS_PlaySong(int handle, int volume);

int SFX_PlayPatch(void *data, int pitch, int sep, int volume, int flags, int priority);
void SFX_StopPatch(int handle);
int SFX_Playing(int handle);
void SFX_SetOrigin(int handle, int pitch, int sep, int volume);

int ENS_Detect(void);
int CODEC_Detect(int *sbPort, int *sbDma);
int GF1_Detect(void);
void GF1_SetMap(char *dmxlump, int size);
int SB_Detect(int *sbPort, int *sbIrq, int *sbDma, uint16_t *version);
void SB_SetCard(int iBaseAddr, int iIrq, int iDma);
int AL_Detect(int *wait, int *type);
void AL_SetCard(int wait, void *genmidi);
int MPU_Detect(int *mPort, int *type);
void MPU_SetCard(int mPort);

#define AHW_PC_SPEAKER		0x0001L
#define AHW_ADLIB			0x0002L
#define AHW_AWE32			0x0004L
#define AHW_SOUND_BLASTER	0x0008L
#define AHW_MPU_401			0x0010L
#define AHW_ULTRA_SOUND		0x0020L
#define AHW_MEDIA_VISION	0x0040L

#define AHW_ENSONIQ 		0x0100L
#define AHW_CODEC           0x0200L

int DMX_Init(int ticrate, int maxsongs, unsigned int musicDevice, unsigned int sfxDevice);
void DMX_DeInit(void);

void WAV_PlayMode(int channels, uint16_t sampleRate);
#endif
