/*
Copyright (C) 1994-1995 Apogee Software, Ltd.
Copyright (C) 2023-2025 Frenkel Smeijers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/**********************************************************************
   module: MUSIC.C

   author: James R. Dose
   date:   March 25, 1994

   Device independant music playback routines.

   (c) Copyright 1994 James R. Dose.  All Rights Reserved.
**********************************************************************/

#include "id_heads.h"
#include "dmx.h"
#include "a_al_mid.h"
#include "a_blast.h"
#include "a_midi.h"
#include "a_mpu401.h"
#include "a_music.h"
#include "a_taskmn.h"

static int32_t MUSIC_SoundDevice = AHW_NONE;

static midifuncs MUSIC_MidiFunctions;

static int32_t MUSIC_InitFM(midifuncs *Funcs);
static int32_t MUSIC_InitMidi(midifuncs *Funcs, int32_t Address);


/*---------------------------------------------------------------------
   Function: MUSIC_Init

   Selects which sound device to use.
---------------------------------------------------------------------*/

int32_t MUSIC_Init(int32_t SoundCard, int32_t Address)
{
	MUSIC_SoundDevice = SoundCard;

	switch (SoundCard)
	{
		case AHW_ADLIB:
			return MUSIC_InitFM(&MUSIC_MidiFunctions);

		case AHW_MPU_401:
			return MUSIC_InitMidi(&MUSIC_MidiFunctions, Address);

		default:
			return MUSIC_Error;
	}
}


/*---------------------------------------------------------------------
   Function: MUSIC_Shutdown

   Terminates use of sound device.
---------------------------------------------------------------------*/

void MUSIC_Shutdown(void)
{
	MIDI_StopSong();

	switch (MUSIC_SoundDevice)
	{
		case AHW_ADLIB :
			AL_Shutdown();
			break;

		case AHW_MPU_401:
			MPU_Reset();
			break;
	}
}


/*---------------------------------------------------------------------
   Function: MUSIC_SetVolume

   Sets the volume of music playback.
---------------------------------------------------------------------*/

void MUSIC_SetVolume(int32_t volume)
{
	if (MUSIC_SoundDevice == AHW_NONE)
		return;

	if (volume < 0)
		volume = 0;
	else if (volume > 255)
		volume = 255;

	MIDI_SetVolume(volume);
}


/*---------------------------------------------------------------------
   Function: MUSIC_Continue

   Continues playback of a paused song.
---------------------------------------------------------------------*/

void MUSIC_Continue(void)
{
	MIDI_ContinueSong();
}


/*---------------------------------------------------------------------
   Function: MUSIC_Pause

   Pauses playback of a song.
---------------------------------------------------------------------*/

void MUSIC_Pause(void)
{
	MIDI_PauseSong();
}


/*---------------------------------------------------------------------
   Function: MUSIC_StopSong

   Stops playback of current song.
---------------------------------------------------------------------*/

void MUSIC_StopSong(void)
{
	MIDI_StopSong();
}


/*---------------------------------------------------------------------
   Function: MUSIC_PlaySong

   Begins playback of MIDI song.
---------------------------------------------------------------------*/

int32_t MUSIC_PlaySong(uint8_t *song, int32_t loopflag)
{
	int32_t status;

	switch (MUSIC_SoundDevice)
	{
		case AHW_ADLIB:
		case AHW_MPU_401:
			MIDI_StopSong();
			status = MIDI_PlaySong(song, loopflag);
			if (status != MIDI_Ok)
				return MUSIC_Warning;
			break;

		default:
			return MUSIC_Warning;
	}

	return MUSIC_Ok;
}


static int32_t MUSIC_InitFM(midifuncs *Funcs)
{
	if (!AL_DetectFM())
		return MUSIC_Error;

	// Init the fm routines
	AL_Init();

	Funcs->NoteOff           = AL_NoteOff;
	Funcs->NoteOn            = AL_NoteOn;
	Funcs->PolyAftertouch    = NULL;
	Funcs->ControlChange     = AL_ControlChange;
	Funcs->ProgramChange     = AL_ProgramChange;
	Funcs->ChannelAftertouch = NULL;
	Funcs->PitchBend         = AL_SetPitchBend;
	Funcs->SetVolume         = NULL;
	Funcs->GetVolume         = NULL;

	MIDI_SetMidiFuncs(Funcs, MUSIC_SoundDevice);

	return MUSIC_Ok;
}


static int32_t MUSIC_InitMidi(midifuncs *Funcs, int32_t Address)
{
	BLASTER_SetupWaveBlaster();

	if (MPU_Init(Address) != MPU_Ok)
		return MUSIC_Error;

	Funcs->NoteOff           = MPU_NoteOff;
	Funcs->NoteOn            = MPU_NoteOn;
	Funcs->PolyAftertouch    = MPU_PolyAftertouch;
	Funcs->ControlChange     = MPU_ControlChange;
	Funcs->ProgramChange     = MPU_ProgramChange;
	Funcs->ChannelAftertouch = MPU_ChannelAftertouch;
	Funcs->PitchBend         = MPU_PitchBend;
	Funcs->SetVolume         = NULL;
	Funcs->GetVolume         = NULL;

	MIDI_SetMidiFuncs(Funcs, MUSIC_SoundDevice);

	return MUSIC_Ok;
}
