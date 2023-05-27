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

#include <conio.h>
#include <dos.h>
#include "doomdef.h"
#include "pcfx.h"
#include "task_man.h"

#define PCFX_MinVoiceHandle 1

static void PCFX_Service(task *Task);

static int32_t	PCFX_LengthLeft;
static uint8_t	*PCFX_Sound = NULL;
static int32_t	PCFX_LastSample;
static task		*PCFX_ServiceTask = NULL;
static int32_t	PCFX_VoiceHandle = PCFX_MinVoiceHandle;

static boolean	PCFX_Installed = false;


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

void PCFX_Stop(int32_t handle)
{
	uint32_t flags;

	if ((handle != PCFX_VoiceHandle) || (PCFX_Sound == NULL))
		return;

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
	uint32_t value;

	UNUSED(Task);

	if (PCFX_Sound)
	{
		value = *(int16_t *)PCFX_Sound;
		PCFX_Sound += sizeof(int16_t);

		if (value != PCFX_LastSample)
		{
			PCFX_LastSample = value;
			if (value)
			{
				outp(0x43, 0xb6);
				outp(0x42, LOBYTE(value));
				outp(0x42, HIBYTE(value));
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

typedef	struct
{
	uint32_t	length;
	uint8_t		data[];
} PCSound;

static int32_t MY_PCFX_Play(PCSound *sound)
{
	uint32_t flags;

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

static uint16_t divisors[] = {
	0,
	6818, 6628, 6449, 6279, 6087, 5906, 5736, 5575,
	5423, 5279, 5120, 4971, 4830, 4697, 4554, 4435,
	4307, 4186, 4058, 3950, 3836, 3728, 3615, 3519,
	3418, 3323, 3224, 3131, 3043, 2960, 2875, 2794,
	2711, 2633, 2560, 2485, 2415, 2348, 2281, 2213,
	2153, 2089, 2032, 1975, 1918, 1864, 1810, 1757,
	1709, 1659, 1612, 1565, 1521, 1478, 1435, 1395,
	1355, 1316, 1280, 1242, 1207, 1173, 1140, 1107,
	1075, 1045, 1015,  986,  959,  931,  905,  879,
	 854,  829,  806,  783,  760,  739,  718,  697,
	 677,  658,  640,  621,  604,  586,  570,  553,
	 538,  522,  507,  493,  479,  465,  452,  439,
	 427,  415,  403,  391,  380,  369,  359,  348,
	 339,  329,  319,  310,  302,  293,  285,  276,
	 269,  261,  253,  246,  239,  232,  226,  219,
	 213,  207,  201,  195,  190,  184,  179,
};

typedef struct {
	uint32_t	length;
	uint16_t	data[0x10000];
} pcspkmuse_t;

typedef struct {
	uint16_t	id;
	uint16_t	length;
	uint8_t		data[];
} dmxpcs_t;

int32_t PCFX_Play(void *vdata)
{
	dmxpcs_t *dmxpcs = (dmxpcs_t *)vdata;
	uint_fast16_t i;

	pcspkmuse_t pcspkmuse;
	pcspkmuse.length = dmxpcs->length * 2;
	for (i = 0; i < dmxpcs->length; i++)
		pcspkmuse.data[i] = divisors[dmxpcs->data[i]];

	return MY_PCFX_Play((PCSound *)&pcspkmuse);
}

/*---------------------------------------------------------------------
   Function: PCFX_SoundPlaying

   Checks if a sound effect is currently playing.
---------------------------------------------------------------------*/

int32_t PCFX_SoundPlaying(int32_t handle)
{
	return (handle == PCFX_VoiceHandle) && (PCFX_LengthLeft > 0);
}


/*---------------------------------------------------------------------
   Function: PCFX_Init

   Initializes the sound effect engine.
---------------------------------------------------------------------*/

void PCFX_Init(int32_t ticrate)
{
	if (PCFX_Installed)
		return;

	PCFX_Stop(PCFX_VoiceHandle);
	PCFX_ServiceTask = TS_ScheduleTask(&PCFX_Service, ticrate, 2, 0);
	TS_Dispatch();

	PCFX_Installed = true;
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
		PCFX_Installed = false;
	}
}
