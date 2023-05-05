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

void _dpmi_lockregion (void * inmem, int length) {UNUSED(inmem); UNUSED(length);}
void _dpmi_unlockregion (void * inmem, int length) {UNUSED(inmem); UNUSED(length);}

int TSM_Install(unsigned int sndTicrate) {UNUSED(sndTicrate); return 0;}
int TSM_NewService(int (*timerISR)(void), int frequency, int priority, int pause) {UNUSED(timerISR); UNUSED(frequency); UNUSED(priority); UNUSED(pause); return 0;}
void TSM_DelService(int tsm_ID) {UNUSED(tsm_ID);}
void TSM_Remove(void) {}

int MUS_PauseSong(int handle) {UNUSED(handle); return 0;}
int MUS_ResumeSong(int handle) {UNUSED(handle); return 0;}
void MUS_SetMasterVolume(int volume) {UNUSED(volume);}
int MUS_RegisterSong(unsigned char *data) {UNUSED(data); return 0;}
int MUS_UnregisterSong(int handle) {UNUSED(handle); return 0;}
int MUS_StopSong(int handle) {UNUSED(handle); return 0;}
int MUS_ChainSong(int handle, int to) {UNUSED(handle); UNUSED(to); return 0;}
int MUS_PlaySong(int handle, int volume) {UNUSED(handle); UNUSED(volume); return 0;}

int SFX_PlayPatch(void *data, int pitch, int sep, int volume, int flags, int priority) {UNUSED(data); UNUSED(pitch); UNUSED(sep); UNUSED(volume); UNUSED(flags); UNUSED(priority); return -1;}
void SFX_StopPatch(int handle) {UNUSED(handle);}
int SFX_Playing(int handle) {UNUSED(handle); return 0;}
void SFX_SetOrigin(int handle, int pitch, int sep, int volume) {UNUSED(handle); UNUSED(pitch); UNUSED(sep); UNUSED(volume);}

int ENS_Detect(void) {return -1;}
int CODEC_Detect(int *sbPort, int *sbDma) {UNUSED(sbPort); UNUSED(sbDma); return -1;}
int GF1_Detect(void) {return -1;}
void GF1_SetMap(char *dmxlump, int size) {UNUSED(dmxlump); UNUSED(size);}
int SB_Detect(int *sbPort, int *sbIrq, int *sbDma, unsigned short *version) {UNUSED(sbPort); UNUSED(sbIrq); UNUSED(sbDma); UNUSED(version); return -1;}
void SB_SetCard(int iBaseAddr, int iIrq, int iDma) {UNUSED(iBaseAddr); UNUSED(iIrq); UNUSED(iDma);}
int AL_Detect(int *wait, int *type) {UNUSED(wait); UNUSED(type); return -1;}
void AL_SetCard(int wait, void *genmidi) {UNUSED(wait); UNUSED(genmidi);}
int MPU_Detect(int *mPort, int *type) {UNUSED(mPort); UNUSED(type); return -1;}
void MPU_SetCard(int mPort) {UNUSED(mPort);}

int DMX_Init(int ticrate, int maxsongs, unsigned int musicDevice, unsigned int sfxDevice) {UNUSED(ticrate); UNUSED(maxsongs); UNUSED(musicDevice); UNUSED(sfxDevice); return 0;}
void DMX_DeInit(void) {}

void WAV_PlayMode(int channels, unsigned short sampleRate) {UNUSED(channels); UNUSED(sampleRate);}
