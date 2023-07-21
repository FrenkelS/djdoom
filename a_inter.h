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
   module: INTERRUP.H

   author: James R. Dose
   date:   March 31, 1994

   Inline functions for disabling and restoring the interrupt flag.

   (c) Copyright 1994 James R. Dose.  All Rights Reserved.
**********************************************************************/

#include <stdint.h>

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
