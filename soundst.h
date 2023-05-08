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

// soundst.h

#ifndef __SOUNDSTH__
#define __SOUNDSTH__

#define S_MAX_VOLUME		127

// when to clip out sounds
// Doesn't fit the large outdoor areas.
#define S_CLIPPING_DIST		(1200*0x10000)

// when sounds should be max'd out
#define S_CLOSE_DIST		(200*0x10000)


#define S_ATTENUATOR		((S_CLIPPING_DIST-S_CLOSE_DIST)>>FRACBITS)

#define NORM_PITCH     		128
#define NORM_VOLUME    		snd_MaxVolume

#define S_PITCH_PERTURB		1
#define NORM_SEP			128
#define S_STEREO_SWING		(96*0x10000)

// % attenuation from front to back
#define S_IFRACVOL			30

#define NA				0
#define S_NUMCHANNELS		2

typedef struct
{
	char *name; // up to 6-character name
	int32_t lumpnum; // lump number of music
	void *data; // music data
	int32_t handle; // music handle once registered
} musicinfo_t;

typedef struct sfxinfo_s
{
	char *name; // up to 6-character name
	int32_t singularity; // Sfx singularity (only one at a time)
	int32_t priority; // Sfx priority
	struct sfxinfo_s *link; // referenced sound if a link
	int32_t pitch; // pitch if a link
	int32_t volume; // volume if a link
	void *data; // sound data
	int32_t usefulness; // Determines when a sound should be cached out
	int32_t lumpnum; // lump number of sfx
} sfxinfo_t;

typedef struct
{
	sfxinfo_t *sfxinfo; // sound information (if null, channel avail.)
	void *origin; // origin of sound
	int32_t handle; // handle of the sound being played
} channel_t;



enum
{
    Music,
    Sfx,
    SfxLink
};

enum
{
    PC=1,
    Adlib=2,
    SB=4,
    Midi=8
}; // cards available

enum
{
    sfxThrowOut=-1,
    sfxNotUsed=0
};

void S_Start(void);
void S_StartSound(void *origin, int32_t sound_id);
void S_StopSound(void *origin);
void S_StartMusic(int32_t music_id);
void S_ChangeMusic(int32_t music_id, boolean looping);
void S_PauseSound(void);
void S_ResumeSound(void);
void S_UpdateSounds(void *listener);
void S_SetMusicVolume(int32_t volume);
void S_SetSfxVolume(int32_t volume);
void S_Init(int32_t,int32_t);

//--------
//SOUND IO
//--------
#define FREQ_LOW		0x40
#define FREQ_NORM		0x80
#define FREQ_HIGH		0xff


void I_SetMusicVolume(int32_t volume);

//
//  MUSIC I/O
//
void I_PauseSong(int32_t handle);
void I_ResumeSong(int32_t handle);

//
// Called by anything that wishes to start music.
//  plays a song, and when the song is done,
//  starts playing it again in an endless loop.
// Horrible thing to do, considering.
void I_PlaySong(int32_t handle, boolean looping);


// stops a song over 3 seconds.
void I_StopSong(int32_t handle);

// registers a song handle to song data
int32_t I_RegisterSong(void *data);

// see above then think backwards
void I_UnRegisterSong(int32_t handle);


//
//  SFX I/O
//
void I_SetChannels(int32_t channels);

int32_t I_GetSfxLumpNum (sfxinfo_t*);


// Starts a sound in a particular sound channel.
int32_t I_StartSound(void *data, int32_t vol, int32_t sep, int32_t pitch);


// Updates the volume, separation,
//  and pitch of a sound channel.
void I_UpdateSoundParams(int32_t handle, int32_t vol, int32_t sep, int32_t pitch);


// Stops a sound channel.
void I_StopSound(int32_t handle);

// Called by S_*()'s to see if a channel is still playing.
// Returns false if no longer playing, true if playing.
boolean I_SoundIsPlaying(int32_t handle);


// the complete set of sound effects
extern sfxinfo_t S_sfx[];

// the complete set of music
extern musicinfo_t S_music[];

#endif
