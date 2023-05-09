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

void _dpmi_lockregion (void * inmem, int32_t length) {UNUSED(inmem); UNUSED(length);}
void _dpmi_unlockregion (void * inmem, int32_t length) {UNUSED(inmem); UNUSED(length);}

int32_t TSM_Install(uint32_t sndTicrate) {UNUSED(sndTicrate); return 0;}
int32_t TSM_NewService(int32_t (*timerISR)(void), int32_t frequency, int32_t priority, int32_t pause) {UNUSED(timerISR); UNUSED(frequency); UNUSED(priority); UNUSED(pause); return 0;}
void TSM_DelService(int32_t tsm_ID) {UNUSED(tsm_ID);}
void TSM_Remove(void) {}

int32_t MUS_PauseSong(int32_t handle) {UNUSED(handle); return 0;}
int32_t MUS_ResumeSong(int32_t handle) {UNUSED(handle); return 0;}
void MUS_SetMasterVolume(int32_t volume) {UNUSED(volume);}
int32_t MUS_RegisterSong(uint8_t *data) {UNUSED(data); return 0;}
int32_t MUS_UnregisterSong(int32_t handle) {UNUSED(handle); return 0;}
int32_t MUS_StopSong(int32_t handle) {UNUSED(handle); return 0;}
int32_t MUS_ChainSong(int32_t handle, int32_t to) {UNUSED(handle); UNUSED(to); return 0;}
int32_t MUS_PlaySong(int32_t handle, int32_t volume) {UNUSED(handle); UNUSED(volume); return 0;}

int32_t SFX_PlayPatch(void *data, int32_t pitch, int32_t sep, int32_t volume, int32_t flags, int32_t priority) {UNUSED(data); UNUSED(pitch); UNUSED(sep); UNUSED(volume); UNUSED(flags); UNUSED(priority); return -1;}
void SFX_StopPatch(int32_t handle) {UNUSED(handle);}
int32_t SFX_Playing(int32_t handle) {UNUSED(handle); return 0;}
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

int32_t DMX_Init(int32_t ticrate, int32_t maxsongs, uint32_t musicDevice, uint32_t sfxDevice) {UNUSED(ticrate); UNUSED(maxsongs); UNUSED(musicDevice); UNUSED(sfxDevice); return 0;}
void DMX_DeInit(void) {}

void WAV_PlayMode(int32_t channels, uint16_t sampleRate) {UNUSED(channels); UNUSED(sampleRate);}
