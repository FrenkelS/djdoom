/*
Copyright (C) 1994-1995 Apogee Software, Ltd.
Copyright (C) 2023 Frenkel Smeijers

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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "task_man.h"
#include "sndcards.h"
#include "music.h"
#include "midi.h"
//#include "al_midi.h"
//#include "pas16.h"
//#include "blaster.h"
//#include "gusmidi.h"
#include "mpu401.h"
//#include "awe32.h"
//#include "sndscape.h"

int32_t MUSIC_SoundDevice = -1;

static midifuncs MUSIC_MidiFunctions;

static int32_t MUSIC_InitAWE32(midifuncs *Funcs);
static int32_t MUSIC_InitFM(int32_t card, midifuncs *Funcs);
static int32_t MUSIC_InitMidi(int32_t card, midifuncs *Funcs, int32_t Address);
static int32_t MUSIC_InitGUS(midifuncs *Funcs);

/*---------------------------------------------------------------------
   Function: MUSIC_Init

   Selects which sound device to use.
---------------------------------------------------------------------*/

int32_t MUSIC_Init(int32_t SoundCard, int32_t Address)
{
	int32_t status = MUSIC_Ok;

	MUSIC_SoundDevice = SoundCard;

	switch (SoundCard)
	{
		case SoundBlaster:
		case Adlib:
		case ProAudioSpectrum:
		case SoundMan16:
			status = MUSIC_InitFM(SoundCard, &MUSIC_MidiFunctions);
			break;

		case GenMidi:
		case SoundCanvas:
		case WaveBlaster:
		case SoundScape:
			status = MUSIC_InitMidi(SoundCard, &MUSIC_MidiFunctions, Address);
			break;

		case Awe32 :
			status = MUSIC_InitAWE32(&MUSIC_MidiFunctions);
			break;

		case UltraSound :
			status = MUSIC_InitGUS(&MUSIC_MidiFunctions);
			break;

		case SoundSource :
		case PC :
		default :
			status = MUSIC_Error;
	}

	return status;
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
/*
		case Adlib:
			AL_Shutdown();
			break;

		case SoundBlaster:
			AL_Shutdown();
			BLASTER_RestoreMidiVolume();
			break;
*/
		case GenMidi:
		case SoundCanvas:
		case SoundScape:
			MPU_Reset();
			break;
/*
		case WaveBlaster:
			BLASTER_ShutdownWaveBlaster();
			MPU_Reset();
			BLASTER_RestoreMidiVolume();
			break;

		case Awe32:
			AWE32_Shutdown();
			BLASTER_RestoreMidiVolume();
			break;

		case ProAudioSpectrum:
		case SoundMan16:
			AL_Shutdown();
			PAS_RestoreMusicVolume();
			break;

		case UltraSound:
			GUSMIDI_Shutdown();
			break;
*/
	}
}


/*---------------------------------------------------------------------
   Function: MUSIC_SetVolume

   Sets the volume of music playback.
---------------------------------------------------------------------*/

void MUSIC_SetVolume(int32_t volume)
{
	if (volume < 0)
		volume = 0;
	if (volume > 255)
		volume = 255;

	if (MUSIC_SoundDevice != -1)
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
		case SoundBlaster:
		case Adlib:
		case ProAudioSpectrum:
		case SoundMan16:
		case GenMidi:
		case SoundCanvas:
		case WaveBlaster:
		case SoundScape:
		case Awe32:
		case UltraSound:
			MIDI_StopSong();
			status = MIDI_PlaySong(song, loopflag);
			if (status != MIDI_Ok)
				return MUSIC_Warning;
			break;

		case SoundSource:
		case PC:
		default:
			return MUSIC_Warning;
	}

	return MUSIC_Ok;
}


static int32_t MUSIC_InitAWE32(midifuncs *Funcs)
{
/*
	int32_t status = AWE32_Init();
	if (status != AWE32_Ok)
		return MUSIC_Error;

	Funcs->NoteOff           = AWE32_NoteOff;
	Funcs->NoteOn            = AWE32_NoteOn;
	Funcs->PolyAftertouch    = AWE32_PolyAftertouch;
	Funcs->ControlChange     = AWE32_ControlChange;
	Funcs->ProgramChange     = AWE32_ProgramChange;
	Funcs->ChannelAftertouch = AWE32_ChannelAftertouch;
	Funcs->PitchBend         = AWE32_PitchBend;
	Funcs->ReleasePatches    = NULL;
	Funcs->LoadPatch         = NULL;
	Funcs->SetVolume         = NULL;
	Funcs->GetVolume         = NULL;

	if (BLASTER_CardHasMixer())
	{
		BLASTER_SaveMidiVolume();
		Funcs->SetVolume = BLASTER_SetMidiVolume;
		Funcs->GetVolume = BLASTER_GetMidiVolume;
	}

	MIDI_SetMidiFuncs(Funcs);

	return MUSIC_Ok;
*/
	return MUSIC_Error;
}


static int32_t MUSIC_InitFM(int32_t card, midifuncs *Funcs)
{
/*
	int32_t passtatus;

	if (!AL_DetectFM())
		return MUSIC_Error;

	// Init the fm routines
	AL_Init(card);

	Funcs->NoteOff           = AL_NoteOff;
	Funcs->NoteOn            = AL_NoteOn;
	Funcs->PolyAftertouch    = NULL;
	Funcs->ControlChange     = AL_ControlChange;
	Funcs->ProgramChange     = AL_ProgramChange;
	Funcs->ChannelAftertouch = NULL;
	Funcs->PitchBend         = AL_SetPitchBend;
	Funcs->ReleasePatches    = NULL;
	Funcs->LoadPatch         = NULL;
	Funcs->SetVolume         = NULL;
	Funcs->GetVolume         = NULL;

	switch (card)
	{
		case SoundBlaster:
			if (BLASTER_CardHasMixer())
			{
				BLASTER_SaveMidiVolume();
				Funcs->SetVolume = BLASTER_SetMidiVolume;
				Funcs->GetVolume = BLASTER_GetMidiVolume;
			} else {
				Funcs->SetVolume = NULL;
				Funcs->GetVolume = NULL;
			}
			break;

		case Adlib:
			Funcs->SetVolume = NULL;
			Funcs->GetVolume = NULL;
			break;

		case ProAudioSpectrum:
		case SoundMan16:
			Funcs->SetVolume = NULL;
			Funcs->GetVolume = NULL;

			passtatus = PAS_SaveMusicVolume();
			if (passtatus == PAS_Ok)
			{
				Funcs->SetVolume = PAS_SetFMVolume;
				Funcs->GetVolume = PAS_GetFMVolume;
			}
			break;
	}

	MIDI_SetMidiFuncs(Funcs);

	return MIDI_Ok;
*/
	return MUSIC_Error;
}

static void BLASTER_SetupWaveBlaster(int32_t unk) {UNUSED(unk);}
#define BLASTER_Ok 0
static int32_t MUSIC_InitMidi(int32_t card, midifuncs *Funcs, int32_t Address)
{
	if (card == WaveBlaster)
	{
/*
		if (Address <= BLASTER_Ok)
			Address = BLASTER_Error;

		Address = BLASTER_SetupWaveBlaster(Address);

		if (Address < BLASTER_Ok)
			return MUSIC_Error;
*/
	}
	else if ((card == SoundCanvas) || (card == GenMidi))
		BLASTER_SetupWaveBlaster(BLASTER_Ok);

/*
	if (card == SoundScape)
	{
		Address = SOUNDSCAPE_GetMIDIPort();
		if (Address < SOUNDSCAPE_Ok)
			return MUSIC_Error;
	}
*/

	if (MPU_Init(Address) != MPU_Ok)
		return MUSIC_Error;

	Funcs->NoteOff           = MPU_NoteOff;
	Funcs->NoteOn            = MPU_NoteOn;
	Funcs->PolyAftertouch    = MPU_PolyAftertouch;
	Funcs->ControlChange     = MPU_ControlChange;
	Funcs->ProgramChange     = MPU_ProgramChange;
	Funcs->ChannelAftertouch = MPU_ChannelAftertouch;
	Funcs->PitchBend         = MPU_PitchBend;
	Funcs->ReleasePatches    = NULL;
	Funcs->LoadPatch         = NULL;
	Funcs->SetVolume         = NULL;
	Funcs->GetVolume         = NULL;

/*
	if (card == WaveBlaster)
	{
		if (BLASTER_CardHasMixer())
		{
			BLASTER_SaveMidiVolume();
			Funcs->SetVolume = BLASTER_SetMidiVolume;
			Funcs->GetVolume = BLASTER_GetMidiVolume;
		}
	}
*/

	MIDI_SetMidiFuncs(Funcs);

	return MUSIC_Ok;
}

static int32_t MUSIC_InitGUS(midifuncs *Funcs)
{
/*
	if (GUSMIDI_Init() != GUS_Ok)
		return MUSIC_Error;

	Funcs->NoteOff           = GUSMIDI_NoteOff;
	Funcs->NoteOn            = GUSMIDI_NoteOn;
	Funcs->PolyAftertouch    = NULL;
	Funcs->ControlChange     = GUSMIDI_ControlChange;
	Funcs->ProgramChange     = GUSMIDI_ProgramChange;
	Funcs->ChannelAftertouch = NULL;
	Funcs->PitchBend         = GUSMIDI_PitchBend;
	Funcs->ReleasePatches    = NULL;
	Funcs->LoadPatch         = NULL;
	Funcs->SetVolume         = GUSMIDI_SetVolume;
	Funcs->GetVolume         = GUSMIDI_GetVolume;

	MIDI_SetMidiFuncs(Funcs);

	return MUSIC_Ok;
*/
	return MUSIC_Error;
}
