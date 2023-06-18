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
   module: FX_MAN.H

   author: James R. Dose
   date:   March 17, 1994

   Public header for FX_MAN.C

   (c) Copyright 1994 James R. Dose.  All Rights Reserved.
**********************************************************************/

#ifndef __FX_MAN_H
#define __FX_MAN_H

#include "sndcards.h"

typedef struct
   {
   int MaxVoices;
   int MaxSampleBits;
   int MaxChannels;
   } fx_device;

#define MonoFx   1
#define StereoFx 2

typedef struct
   {
   unsigned long Address;
   unsigned long Type;
   unsigned long Interrupt;
   unsigned long Dma8;
   unsigned long Dma16;
   unsigned long Midi;
   unsigned long Emu;
   } fx_blaster_config;

enum FX_ERRORS
   {
   FX_Warning = -2,
   FX_Error = -1,
   FX_Ok = 0,
   FX_ASSVersion,
   FX_BlasterError,
   FX_SoundCardError,
   FX_InvalidCard,
   FX_MultiVocError,
   FX_DPMI_Error
   };

enum fx_BLASTER_Types
   {
   fx_SB     = 1,
   fx_SBPro  = 2,
   fx_SB20   = 3,
   fx_SBPro2 = 4,
   fx_SB16   = 6
   };


int  FX_GetBlasterSettings( fx_blaster_config *blaster );
void FX_SetupSoundBlaster( fx_blaster_config blaster, int *MaxVoices, int *MaxSampleBits, int *MaxChannels );
void FX_Init( int SoundCard, int numvoices, int numchannels, int samplebits, unsigned mixrate );
void FX_Shutdown( void );
void FX_SetVolume( int volume );

void FX_SetPan( int handle, int vol, int left, int right );
void FX_SetPitch( int handle, int pitchoffset );

int  FX_PlayRaw( char *ptr, unsigned long length, unsigned rate, int pitchoffset, int vol, int left, int right, int priority, unsigned long callbackval );
int  FX_SoundActive( int handle );
void FX_StopSound( int handle );

#endif
