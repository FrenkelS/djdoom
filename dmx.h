//
// Copyright (C) 2015 Alexey Khokholov (Nuke.YKT)
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

#ifndef _DMX_H_
#define _DMX_H_

#include "tsmapi.h"

int AL_DetectFM(void);
int MPU_Init(int addr);
int GUS_Init(void);
void GUS_Shutdown(void);

void MUS_PauseSong(int handle);
void MUS_ResumeSong(int handle);
void MUS_SetMasterVolume(int volume);
int MUS_RegisterSong(void *data);
int MUS_UnregisterSong(int handle);
int MUS_StopSong(int handle);
int MUS_ChainSong(int handle, int next);
int MUS_PlaySong(int handle, int volume);
int SFX_PlayPatch(void *vdata, int pitch, int sep, int vol, int unk1, int unk2);
void SFX_StopPatch(int handle);
int SFX_Playing(int handle);
void SFX_SetOrigin(int handle, int  pitch, int sep, int vol);
int GF1_Detect(void);
void GF1_SetMap(void *data, int len);
int SB_Detect(int *port, int *irq, int *dma, int *unk);
void SB_SetCard(int port, int irq, int dma);
int AL_Detect(int *port, int *unk);
void AL_SetCard(int port, void *data);
int MPU_Detect(int *port, int *unk);
void MPU_SetCard(int port);
int DMX_Init(int rate, int maxsng, int mdev, int sdev);
void DMX_DeInit(void);
void WAV_PlayMode(int channels, int samplerate);
int CODEC_Detect(int *a, int *b);
int ENS_Detect(void);


#define AHW_PC_SPEAKER		0x0001L
#define AHW_ADLIB			0x0002L
#define AHW_AWE32			0x0004L
#define AHW_SOUND_BLASTER	0x0008L
#define AHW_MPU_401			0x0010L
#define AHW_ULTRA_SOUND		0x0020L
#define AHW_MEDIA_VISION	0x0040L

#define AHW_ENSONIQ 		0x0100L
#define AHW_CODEC			0x0200L

#endif
