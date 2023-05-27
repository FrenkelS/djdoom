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
   module: PCFX.C

   author: James R. Dose
   date:   April 1, 1994

   Low level routines to support PC sound effects created by Muse.

   (c) Copyright 1994 James R. Dose.  All Rights Reserved.
**********************************************************************/

#include <dos.h>
#include <stdlib.h>
#include <conio.h>
#include "doomdef.h"
#include "pcfx.h"
#include "task_man.h"

#define TRUE  ( 1 == 1 )
#define FALSE ( !TRUE )

static void PCFX_Service(task *Task);

static long   PCFX_LengthLeft;
static char  *PCFX_Sound = NULL;
static int    PCFX_LastSample;
static int    PCFX_TotalVolume = PCFX_MaxVolume;
static task  *PCFX_ServiceTask = NULL;
static int    PCFX_VoiceHandle = PCFX_MinVoiceHandle;

static int PCFX_Installed = FALSE;

static int PCFX_ErrorCode = PCFX_Ok;

#define PCFX_SetErrorCode(status) \
   PCFX_ErrorCode   = (status);


static uint32_t DisableInterrupts(void);
static void RestoreInterrupts(uint32_t flags);


#if defined __DMC__ || defined __CCDL__
static uint32_t DisableInterrupts(void)
{
	asm
	{
		pushfd
		pop eax
		cli
	}
	return _EAX;
}

static void RestoreInterrupts(uint32_t flags)
{
	asm
	{
		mov eax, [flags]
		push eax
		popfd
	}
}

#elif defined __DJGPP__
static uint32_t DisableInterrupts(void)
{
	uint32_t a;
	asm
	(
		"pushfl \n"
		"popl %0 \n"
		"cli"
		: "=r" (a)
	);
	return a;
}

static void RestoreInterrupts(uint32_t flags)
{
	asm
	(
		"pushl %0 \n"
		"popfl"
		:
		: "r" (flags)
	);
}

#elif defined __WATCOMC__
#pragma aux DisableInterrupts =	\
	"pushfd",					\
	"pop eax",					\
	"cli"						\
	value [eax];

#pragma aux RestoreInterrupts =	\
	"push eax",					\
	"popfd"						\
	parm [eax];
#endif


/*---------------------------------------------------------------------
   Function: PCFX_Stop

   Halts playback of the currently playing sound effect.
---------------------------------------------------------------------*/

void PCFX_Stop(int handle)
{
	unsigned flags;

	if ((handle != PCFX_VoiceHandle) || (PCFX_Sound == NULL))
	{
		PCFX_SetErrorCode(PCFX_VoiceNotFound);
		return;
	}

	flags = DisableInterrupts();

	// Turn off speaker
	outp(0x61, inp(0x61) & 0xfc);

	PCFX_Sound      = NULL;
	PCFX_LengthLeft = 0;
	PCFX_LastSample = 0;

	RestoreInterrupts(flags);
}


/*---------------------------------------------------------------------
   Function: PCFX_Service

   Task Manager routine to perform the playback of a sound effect.
---------------------------------------------------------------------*/

static void PCFX_Service(task *Task)
{
	unsigned value;

	UNUSED(Task);

	if (PCFX_Sound)
	{
		value = *(short int *)PCFX_Sound;
		PCFX_Sound += sizeof(short int);

		if ((PCFX_TotalVolume > 0) && (value != PCFX_LastSample))
		{
			PCFX_LastSample = value;
			if (value)
			{
				outp(0x43, 0xb6);
				outp(0x42, value);
				outp(0x42, value >> 8);
				outp(0x61, inp(0x61) | 0x3);
			} else
				outp(0x61, inp(0x61) & 0xfc);
		}

		if (--PCFX_LengthLeft == 0)
			PCFX_Stop(PCFX_VoiceHandle);
	}
}


/*---------------------------------------------------------------------
   Function: PCFX_Play

   Starts playback of a Muse sound effect.
---------------------------------------------------------------------*/

int PCFX_Play(PCSound *sound)
{
	unsigned flags;

	PCFX_Stop(PCFX_VoiceHandle);

	PCFX_VoiceHandle++;
	if (PCFX_VoiceHandle < PCFX_MinVoiceHandle)
		PCFX_VoiceHandle = PCFX_MinVoiceHandle;

	flags = DisableInterrupts();

	PCFX_LengthLeft = sound->length;
	PCFX_LengthLeft >>= 1;

	PCFX_Sound = &sound->data;

	RestoreInterrupts(flags);

	return PCFX_VoiceHandle;
}


/*---------------------------------------------------------------------
   Function: PCFX_SoundPlaying

   Checks if a sound effect is currently playing.
---------------------------------------------------------------------*/

int PCFX_SoundPlaying(int handle)
{
	return (handle == PCFX_VoiceHandle) && (PCFX_LengthLeft > 0);
}


/*---------------------------------------------------------------------
   Function: PCFX_SetTotalVolume

   Sets the total volume of the sound effects.
---------------------------------------------------------------------*/

static void PCFX_SetTotalVolume(int volume)
{
	unsigned flags = DisableInterrupts();

	volume = max(volume, 0);
	volume = min(volume, PCFX_MaxVolume);

	PCFX_TotalVolume = volume;

	if (volume == 0)
		outp(0x61, inp(0x61) & 0xfc);

	RestoreInterrupts(flags);
}


/*---------------------------------------------------------------------
   Function: PCFX_Init

   Initializes the sound effect engine.
---------------------------------------------------------------------*/

void PCFX_Init(void)
{
	if (PCFX_Installed)
		PCFX_Shutdown();

	PCFX_Stop(PCFX_VoiceHandle);
	PCFX_ServiceTask = TS_ScheduleTask(&PCFX_Service, 140, 2, 0);
	TS_Dispatch();

	PCFX_Installed = TRUE;

	PCFX_SetErrorCode(PCFX_Ok);

	PCFX_SetTotalVolume(255);
}


/*---------------------------------------------------------------------
   Function: PCFX_Shutdown

   Ends the use of the sound effect engine.
---------------------------------------------------------------------*/

void PCFX_Shutdown(void)
{
	if (PCFX_Installed)
	{
		PCFX_Stop(PCFX_VoiceHandle);
		TS_Terminate(PCFX_ServiceTask);
		PCFX_Installed = FALSE;
	}

	PCFX_SetErrorCode(PCFX_Ok);
}
