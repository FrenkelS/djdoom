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

#include "doomdef.h"
#include "r_local.h"
#include "soundst.h"

static channel_t *channels; // the set of channels available
static int32_t snd_SfxVolume;
static boolean mus_paused;	// whether songs are mus_paused
static musicinfo_t *mus_playing=0;// music currently being played
int32_t numChannels; // number of channels available
static int32_t nextcleanup;
//
// Internals.
//
static int32_t S_getChannel (void *origin, sfxinfo_t *sfxinfo);
static boolean S_AdjustSoundParams ( mobj_t *listener, mobj_t *source, int32_t *vol, int32_t *sep);
static void S_StopChannel(int32_t cnum);

void S_SetMusicVolume(int32_t volume)
{
  if (volume < 0 || volume > S_MAX_VOLUME)
    I_Error("Attempt to set music volume at %d", volume);

  I_SetMusicVolume(volume);
}

static void S_StopMusic(void)
{
  if (mus_playing)
  {
    if (mus_paused)
      I_ResumeSong(mus_playing->handle);
    I_StopSong(mus_playing->handle);
    I_UnRegisterSong(mus_playing->handle);
    Z_ChangeTag(mus_playing->data, PU_CACHE);
    mus_playing->data = 0;
    mus_playing = 0;
  }
}

void S_ChangeMusic (int32_t musicnum, boolean looping)
{
	musicinfo_t *music;
	char namebuf[9];
	extern boolean I_IsAdlib(void);

	if (I_IsAdlib() && musicnum == mus_intro)
		musicnum = mus_introa;

	if ( (musicnum <= mus_None) || (musicnum >= NUMMUSIC) )
	{
		I_Error("Bad music number %d", musicnum);
		return; // shut up warning
	}
	else
		music = &S_music[musicnum];

	if (mus_playing == music)
		return;

	// shutdown old music
	S_StopMusic();

	// get lumpnum if neccessary
	if (!music->lumpnum)
	{
		sprintf(namebuf, "d_%s", music->name);
		music->lumpnum = W_GetNumForName(namebuf);
	}

	// load & register it
	music->data = (void *) W_CacheLumpNum(music->lumpnum, PU_MUSIC);
	music->handle = I_RegisterSong(music->data);

	// play it
	I_PlaySong(music->handle, looping);
	mus_playing = music;
}

void S_StartMusic(int32_t m_id)
{
    S_ChangeMusic(m_id, false);
}

static void S_StopChannel(int32_t cnum)
{
  int32_t i;
  channel_t *c = &channels[cnum];

  if (c->sfxinfo)
  {
    // stop the sound playing
    if (I_SoundIsPlaying(c->handle))
    {
#ifdef SAWDEBUG
      if (c->sfxinfo == &S_sfx[sfx_sawful])
	fprintf(stderr, "stopped\n");
#endif
      I_StopSound(c->handle);
    }
    // check to see
    //  if other channels are playing the sound
    for (i=0 ; i<numChannels ; i++)
#if (APPVER_DOOMREV < AV_DR_DM1666)
      if (cnum != i && c->sfxinfo == c->sfxinfo)
#else
      if (cnum != i && c->sfxinfo == channels[i].sfxinfo)
#endif
	break;
    // degrade usefulness of sound data
    c->sfxinfo->usefulness--;
    c->sfxinfo = 0;
  }
}

//
// Changes volume and stereo-separation variables
//  from the norm of a sound effect to be played.
// If the sound is not audible, returns false.
// Otherwise, modifies parameters and returns true.
//
static boolean S_AdjustSoundParams (mobj_t *listener, mobj_t *source, int32_t *vol, int32_t *sep)
{
  fixed_t approx_dist, adx, ady;
  angle_t angle;

  // calculate the distance to sound origin and clip it if necessary
  adx = abs(listener->x - source->x);
  ady = abs(listener->y - source->y);
  // From _GG1_ p.428. Appox. eucledian distance fast.
  approx_dist = adx + ady - ((adx < ady ? adx : ady)>>1);
  if (gamemap != 8 && approx_dist > S_CLIPPING_DIST)
    return false;
  // angle of source to listener
  angle = R_PointToAngle2(listener->x, listener->y, source->x, source->y);
  if (angle > listener->angle)
    angle = angle - listener->angle;
  else
    angle = angle + (0xffffffff - listener->angle);
  angle >>= ANGLETOFINESHIFT;
  // stereo separation
  *sep = 128 - (FixedMul(S_STEREO_SWING,finesine[angle])>>FRACBITS);
  // volume calculation
  if (approx_dist < S_CLOSE_DIST)
    *vol = snd_SfxVolume;
  else if (gamemap == 8)
  {
    if (approx_dist > S_CLIPPING_DIST)
      approx_dist = S_CLIPPING_DIST;
    *vol = 15+ ((snd_SfxVolume-15)*((S_CLIPPING_DIST - approx_dist)>>FRACBITS))
      / S_ATTENUATOR;
  }
  else
  {
    // distance effect
    *vol = (snd_SfxVolume * ((S_CLIPPING_DIST - approx_dist)>>FRACBITS))
      / S_ATTENUATOR; 
  }
  return (*vol > 0);
}

void S_SetSfxVolume(int32_t volume)
{
  if (volume < 0 || volume > S_MAX_VOLUME)
    I_Error("Attempt to set sfx volume at %d", volume);
  snd_SfxVolume = volume;
}

//
// Stop and resume music, during game PAUSE.
//
void S_PauseSound(void)
{
  if (mus_playing && !mus_paused)
  {
    I_PauseSong(mus_playing->handle);
    mus_paused = true;
  }
}

void S_ResumeSound(void)
{
  if (mus_playing && mus_paused)
  {
    I_ResumeSong(mus_playing->handle);
    mus_paused = false;
  }
}

void S_StopSound(void *origin)
{
  int32_t cnum;

  for (cnum=0 ; cnum<numChannels ; cnum++)
  {
    if (channels[cnum].sfxinfo && channels[cnum].origin == origin)
    {
      S_StopChannel(cnum);
      break;
    }
  }
}

//
// S_getChannel :
//   If none available, return -1.  Otherwise channel #.
//
static int32_t S_getChannel (void *origin, sfxinfo_t *sfxinfo)
{
  int32_t cnum;// channel number to use
  channel_t *c;

  // Find an open channel
  for (cnum=0 ; cnum<numChannels ; cnum++)
  {
    if (!channels[cnum].sfxinfo)
      break;
    else if (origin &&  channels[cnum].origin ==  origin)
    {
      S_StopChannel(cnum);
      break;
    }
  }
  // None available
  if (cnum == numChannels)
  {
    // Look for lower priority
    for (cnum=0 ; cnum<numChannels ; cnum++)
      if (channels[cnum].sfxinfo->priority >= sfxinfo->priority) break;
	if (cnum == numChannels)
	  return -1; // FUCK!  No lower priority.  Sorry, Charlie.    
	else
	  S_StopChannel(cnum);  // Otherwise, kick out lower priority.
  }
  c = &channels[cnum];
  // channel is decided to be cnum.
  c->sfxinfo = sfxinfo;
  c->origin = origin;
  return cnum;
}

#if (APPVER_DOOMREV < AV_DR_DM17)
static void S_StartSoundAtVolume(mobj_t *origin, int32_t sfx_id, int32_t volume)
#else
static void S_StartSoundAtVolume(void *origin_p, int32_t sfx_id, int32_t volume)
#endif
{
  boolean rc;
  int32_t sep, pitch;
  sfxinfo_t *sfx;
  int32_t cnum;
#if (APPVER_DOOMREV >= AV_DR_DM17)
  mobj_t *origin = (mobj_t *) origin_p;
#endif

  // Debug.
  /*fprintf( stderr,
  	   "S_StartSoundAtVolume: playing sound %d (%s)\n",
  	   sfx_id, S_sfx[sfx_id].name );*/
  // check for bogus sound #
  if (sfx_id < 1 || sfx_id > NUMSFX)
    I_Error("Bad sfx #: %d", sfx_id);
  
  sfx = &S_sfx[sfx_id];
  
  // Initialize sound parameters
  if (sfx->link)
  {
    pitch = sfx->pitch;
    volume += sfx->volume;
    
    if (volume < 1)
      return;
    
    if (volume > snd_SfxVolume)
      volume = snd_SfxVolume;
  }	
  else
  {
    pitch = NORM_PITCH;
  }
  // Check to see if it is audible,
  //  and if not, modify the params
  if (origin && origin != players[consoleplayer].mo)
  {
    rc = S_AdjustSoundParams(players[consoleplayer].mo, origin, &volume, &sep);
    if ( origin->x == players[consoleplayer].mo->x
	 && origin->y == players[consoleplayer].mo->y)
    {	
      sep 	= NORM_SEP;
    }

    if (!rc)
      return;
  }	
  else
  {
    sep = NORM_SEP;
  }
  
  // hacks to vary the sfx pitches
  if (sfx_id >= sfx_sawup && sfx_id <= sfx_sawhit)
  {	
    pitch += 8 - (M_Random()&15);
    if (pitch<0)
      pitch = 0;
    else if (pitch>255)
      pitch = 255;
  }
  else if (sfx_id != sfx_itemup && sfx_id != sfx_tink)
  {
    pitch += 16 - (M_Random()&31);
    if (pitch<0)
      pitch = 0;
    else if (pitch>255)
      pitch = 255;
  }
  // kill old sound
  S_StopSound(origin);
  // try to find a channel
  cnum = S_getChannel(origin, sfx);  
  if (cnum<0)
    return;

  // get lumpnum if necessary
  if (sfx->lumpnum < 0)
    sfx->lumpnum = I_GetSfxLumpNum(sfx);

#ifndef SNDSRV
  // cache data if necessary
  if (!sfx->data)
  {
    sfx->data = (void *) W_CacheLumpNum(sfx->lumpnum, PU_MUSIC);
    // fprintf( stderr,
    //	     "S_StartSoundAtVolume: loading %d (lump %d) : 0x%x\n",
    //       sfx_id, sfx->lumpnum, (int32_t)sfx->data );
  }
#endif
  
  // increase the usefulness
  if (sfx->usefulness++ < 0)
    sfx->usefulness = 1;
  
  // Assigns the handle to one of the channels in the
  //  mix/output buffer.
  channels[cnum].handle = I_StartSound(sfx->data, volume, sep, pitch);
}

void S_StartSound (void *origin, int32_t sfx_id)
{
#ifdef SAWDEBUG
// if (sfx_id == sfx_sawful)
// sfx_id = sfx_itemup;
#endif
  S_StartSoundAtVolume(origin, sfx_id, snd_SfxVolume);
#ifdef SAWDEBUG
{
  int32_t i, n;
  static mobj_t *last_saw_origins[10] = {1,1,1,1,1,1,1,1,1,1};
  static int32_t first_saw=0;
  static int32_t next_saw=0;
	
  if (sfx_id == sfx_sawidl || sfx_id == sfx_sawful || sfx_id == sfx_sawhit)
  {
    for (i=first_saw;i!=next_saw;i=(i+1)%10)
      if (last_saw_origins[i] != origin)
	fprintf(stderr, "old origin 0x%lx != origin 0x%lx for sfx %d\n",
	  last_saw_origins[i], origin, sfx_id);    
    last_saw_origins[next_saw] = origin;
    next_saw = (next_saw + 1) % 10;
    if (next_saw == first_saw)
      first_saw = (first_saw + 1) % 10;
    for (n=i=0; i<numChannels ; i++)
    {
      if (channels[i].sfxinfo == &S_sfx[sfx_sawidl]
	|| channels[i].sfxinfo == &S_sfx[sfx_sawful]
	|| channels[i].sfxinfo == &S_sfx[sfx_sawhit]) n++;
    }
    if (n>1)
    {
      for (i=0; i<numChannels ; i++)
      {
	if (channels[i].sfxinfo == &S_sfx[sfx_sawidl]
	  || channels[i].sfxinfo == &S_sfx[sfx_sawful]
	  || channels[i].sfxinfo == &S_sfx[sfx_sawhit])
	{
	  fprintf(stderr, "chn: sfxinfo=0x%lx, origin=0x%lx, handle=%d\n",
	    channels[i].sfxinfo, channels[i].origin, channels[i].handle);
	}
      }
      fprintf(stderr, "\n");
    }
  }
}
#endif
}

//
// Updates music & sounds
//
#if (APPVER_DOOMREV < AV_DR_DM17)
void S_UpdateSounds(mobj_t* listener)
#else
void S_UpdateSounds(void* listener_p)
#endif
{
  boolean audible;
  int32_t cnum,volume, sep, pitch, i;
  sfxinfo_t *sfx;
  channel_t *c; 
#if (APPVER_DOOMREV >= AV_DR_DM17)
  mobj_t *listener = (mobj_t*)listener_p;
#endif

  if (gametic > nextcleanup)
  {
    for (i=1 ; i<NUMSFX ; i++)
    {
      if (S_sfx[i].usefulness < 1 && S_sfx[i].usefulness > -1)
      {
        if (--S_sfx[i].usefulness == -1)
        {
          Z_ChangeTag(S_sfx[i].data, PU_CACHE);
          S_sfx[i].data = 0;
        }
      }
    }
    nextcleanup = gametic + 15;
  }

  for (cnum=0 ; cnum<numChannels ; cnum++)
  {
    c = &channels[cnum];
    sfx = c->sfxinfo;
    if (c->sfxinfo)
    {
      if (I_SoundIsPlaying(c->handle))
      {
	// initialize parameters
	volume = snd_SfxVolume;
	pitch = NORM_PITCH;
	sep = NORM_SEP;
	if (sfx->link)
	{
	  pitch = sfx->pitch;
	  volume += sfx->volume;
	  if (volume < 1)
	  {
	    S_StopChannel(cnum);
	    continue;
	  }
	  else if (volume > snd_SfxVolume)
	  {
	    volume = snd_SfxVolume;
	  }
	}
	// check non-local sounds for distance clipping
	//  or modify their params
#if (APPVER_DOOMREV < AV_DR_DM17)
	if (c->origin && listener != c->origin)
#else
	if (c->origin && listener_p != c->origin)
#endif
	{
	  audible = S_AdjustSoundParams(listener, c->origin, &volume, &sep);
	  if (!audible)
	  {
	    S_StopChannel(cnum);
	  }
	  else
	    I_UpdateSoundParams(c->handle, volume, sep, pitch);
	}
      }
      else
      {
	// if channel is allocated but sound has stopped,
	//  free it
	S_StopChannel(cnum);
      }
}
}
}

//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//  allocates channel buffer, sets S_sfx lookup.
//
void S_Init (int32_t sfxVolume, int32_t musicVolume)
{  
  int32_t i;

  //fprintf( stderr, "S_Init: default sfx volume %d\n", sfxVolume);

  // Whatever these did with DMX, these are rather dummies now.
  I_SetChannels(numChannels);
  
  S_SetSfxVolume(sfxVolume);
  // No music with Linux - another dummy.
  S_SetMusicVolume(musicVolume);

  // Allocating the internal channels for mixing
  // (the maximum numer of sounds rendered
  // simultaneously) within zone memory.
  channels =
    (channel_t *) Z_Malloc(numChannels*sizeof(channel_t), PU_STATIC, 0);
  
  // Free all channels for use
  for (i=0 ; i<numChannels ; i++)
    channels[i].sfxinfo = 0;
  
  // no sounds are playing, and they are not mus_paused
  mus_paused = 0;

  // Note that sounds have not been cached (yet).
  for (i=1 ; i<NUMSFX ; i++)
    S_sfx[i].lumpnum = S_sfx[i].usefulness = -1;
}

//
// Per level startup code.
// Kills playing sounds at start of level,
//  determines music if any, changes music.
//
void S_Start(void)
{
  int32_t cnum, mnum;

  // kill all playing sounds at start of level
  //  (trust me - a good idea)
  for (cnum=0 ; cnum<numChannels ; cnum++)
    if (channels[cnum].sfxinfo)
      S_StopChannel(cnum);
  
  // start new music for the level
  mus_paused = 0;
  
  if (commercial)
    mnum = mus_runnin + gamemap - 1;
  else
#if (APPVER_DOOMREV < AV_DR_DM19UP)
    mnum = mus_e1m1 + (gameepisode-1)*9 + gamemap-1;
#else
  {
    int32_t spmus[]=
    {
      // Song - Who? - Where?
      
#if (APPVER_DOOMREV < AV_DR_DM19U)
      mus_e2m6,
      mus_e3m2,
      mus_e3m3,
      mus_e1m4,
      mus_e2m7,
      mus_e2m4,
      mus_e2m5,
      mus_e1m6,
      mus_e1m6
#else
      mus_e3m4,	// American	e4m1
      mus_e3m2,	// Romero	e4m2
      mus_e3m3,	// Shawn	e4m3
      mus_e1m5,	// American	e4m4
      mus_e2m7,	// Tim 	e4m5
      mus_e2m4,	// Romero	e4m6
      mus_e2m6,	// J.Anderson	e4m7 CHIRON.WAD
      mus_e2m5,	// Shawn	e4m8
      mus_e1m9	// Tim		e4m9
#endif
    };
    
    if (gameepisode < 4)
      mnum = mus_e1m1 + (gameepisode-1)*9 + gamemap-1;
    else
      mnum = spmus[gamemap-1];
    }	
#endif // AV_DR_DM19UP
  
  // HACK FOR COMMERCIAL
  //  if (commercial && mnum > mus_e3m9)	
  //      mnum -= mus_e3m9;
  
  S_ChangeMusic(mnum, true);
  
  nextcleanup = 15;
}

