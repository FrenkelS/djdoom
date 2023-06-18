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
   module: FX_MAN.C

   author: James R. Dose
   date:   March 17, 1994

   Device independant sound effect routines.

   (c) Copyright 1994 James R. Dose.  All Rights Reserved.
**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "sndcards.h"
//#include "multivoc.h"
#include "blaster.h"
#include "fx_man.h"

#define TRUE  ( 1 == 1 )
#define FALSE ( !TRUE )

static int FX_SoundDevice = -1;
static int FX_Installed = FALSE;


/*---------------------------------------------------------------------
   Function: FX_GetBlasterSettings

   Returns the current BLASTER environment variable settings.
---------------------------------------------------------------------*/

int FX_GetBlasterSettings(fx_blaster_config *blaster)
{
	int status;
	BLASTER_CONFIG Blaster;

	status = BLASTER_GetEnv(&Blaster);
	if (status != BLASTER_Ok)
		return FX_Error;

	blaster->Type      = Blaster.Type;
	blaster->Address   = Blaster.Address;
	blaster->Interrupt = Blaster.Interrupt;
	blaster->Dma8      = Blaster.Dma8;
	blaster->Dma16     = Blaster.Dma16;
	blaster->Midi      = Blaster.Midi;
	blaster->Emu       = Blaster.Emu;

	return FX_Ok;
}


/*---------------------------------------------------------------------
   Function: FX_SetupSoundBlaster

   Handles manual setup of the Sound Blaster information.
---------------------------------------------------------------------*/

void FX_SetupSoundBlaster(fx_blaster_config blaster, int *MaxVoices, int *MaxSampleBits, int *MaxChannels)
{
	int DeviceStatus;
	BLASTER_CONFIG Blaster;

	FX_SoundDevice = SoundBlaster;

	Blaster.Type      = blaster.Type;
	Blaster.Address   = blaster.Address;
	Blaster.Interrupt = blaster.Interrupt;
	Blaster.Dma8      = blaster.Dma8;
	Blaster.Dma16     = blaster.Dma16;
	Blaster.Midi      = blaster.Midi;
	Blaster.Emu       = blaster.Emu;

	BLASTER_SetCardSettings(Blaster);

	DeviceStatus = BLASTER_Init();
	if (DeviceStatus == BLASTER_Ok)
	{
		*MaxVoices = 8;
		BLASTER_GetCardInfo(MaxSampleBits, MaxChannels);
	}
}


/*---------------------------------------------------------------------
   Function: FX_Init

   Selects which sound device to use.
---------------------------------------------------------------------*/

void FX_Init(int SoundCard, int numvoices, int numchannels, int samplebits, unsigned mixrate)
{
	int status;
	int devicestatus;

	if (FX_Installed)
		FX_Shutdown();

	status = FX_Ok;

	FX_SoundDevice = SoundCard;
	switch (SoundCard)
	{
		case SoundBlaster:
			devicestatus = MV_Init(SoundCard, mixrate, numvoices, numchannels, samplebits);
			if (devicestatus != MV_Ok)
				status = FX_Error;
			break;

		default:
			status = FX_Error;
	}

	if (status == FX_Ok)
		FX_Installed = TRUE;
}


/*---------------------------------------------------------------------
   Function: FX_Shutdown

   Terminates use of sound device.
---------------------------------------------------------------------*/

void FX_Shutdown(void)
{
	int status;

	if (!FX_Installed)
		return;

	switch (FX_SoundDevice)
	{
		case SoundBlaster:
			status = MV_Shutdown();
			if (status != MV_Ok)
				status = FX_Error;
			break;
	}

	FX_Installed = FALSE;
}


/*---------------------------------------------------------------------
   Function: FX_SetVolume

   Sets the volume of the current sound device.
---------------------------------------------------------------------*/

void FX_SetVolume(int volume)
{
	switch (FX_SoundDevice)
	{
		case SoundBlaster:
			if (BLASTER_CardHasMixer())
				BLASTER_SetVoiceVolume(volume);
			else
				MV_SetVolume(volume);
			break;
      }
   }


/*---------------------------------------------------------------------
   Function: FX_SetPan

   Sets the stereo and mono volume level of the voice associated
   with the specified handle.
---------------------------------------------------------------------*/

void FX_SetPan(int handle, int vol, int left, int right)
{
	int status;

	switch (FX_SoundDevice)
	{
		case SoundBlaster:
			status = MV_SetPan(handle, vol, left, right);
			if (status == MV_Error)
				status = FX_Warning;
			break;
	}
}


/*---------------------------------------------------------------------
   Function: FX_SetPitch

   Sets the pitch of the voice associated with the specified handle.
---------------------------------------------------------------------*/

void FX_SetPitch(int handle, int pitchoffset)
{
	int status = FX_Ok;

	switch (FX_SoundDevice)
	{
		case SoundBlaster:
			status = MV_SetPitch(handle, pitchoffset);
			if (status == MV_Error)
				status = FX_Warning;
			break;
	}
}


/*---------------------------------------------------------------------
   Function: FX_PlayRaw

   Begin playback of raw sound data with the given volume and priority.
---------------------------------------------------------------------*/

int FX_PlayRaw(char *ptr, unsigned long length, unsigned rate, int pitchoffset, int vol, int left, int right, int priority, unsigned long callbackval)
{
	int handle;

	switch (FX_SoundDevice)
	{
		case SoundBlaster:
			handle = MV_PlayRaw(ptr, length, rate, pitchoffset, vol, left, right, priority, callbackval);
			if (handle < MV_Ok)
				handle = FX_Warning;
			break;

		default:
			handle = FX_Error;
	}

	return handle;
}


/*---------------------------------------------------------------------
   Function: FX_SoundActive

   Tests if the specified sound is currently playing.
---------------------------------------------------------------------*/

int FX_SoundActive(int handle)
{
	switch (FX_SoundDevice)
	{
		case SoundBlaster:
			return MV_VoicePlaying(handle);
	}

	return FALSE;
}


/*---------------------------------------------------------------------
   Function: FX_StopSound

   Halts playback of a specific voice
---------------------------------------------------------------------*/

void FX_StopSound(int handle)
{
	switch( FX_SoundDevice )
	{
		case SoundBlaster :
			MV_Kill( handle );
	}
}
