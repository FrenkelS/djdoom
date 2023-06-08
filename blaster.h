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
   module: BLASTER.H

   author: James R. Dose
   date:   February 4, 1994

   Public header for BLASTER.C

   (c) Copyright 1994 James R. Dose.  All Rights Reserved.
**********************************************************************/

#ifndef __BLASTER_H
#define __BLASTER_H

typedef struct
   {
   unsigned Address;
   unsigned Type;
   unsigned Interrupt;
   unsigned Dma8;
   unsigned Dma16;
   unsigned Midi;
   unsigned Emu;
   } BLASTER_CONFIG;

#define UNDEFINED -1

enum BLASTER_ERRORS
   {
   BLASTER_Error = -1,
   BLASTER_Ok = 0,
   BLASTER_EnvNotFound,
   BLASTER_AddrNotSet,
   BLASTER_IntNotSet,
   BLASTER_DMANotSet,
   BLASTER_DMA16NotSet,
   BLASTER_MIDINotSet,
   BLASTER_CardTypeNotSet,
   BLASTER_InvalidParameter,
   BLASTER_UnsupportedCardType
   };

enum BLASTER_Types
   {
   SB     = 1,
   SBPro  = 2,
   SB20   = 3,
   SBPro2 = 4,
   SB16   = 6
   };

#define BLASTER_MinCardType    SB
#define BLASTER_MaxCardType    SB16

#define STEREO      1
#define SIXTEEN_BIT 2

#define MONO_8BIT    0
#define STEREO_8BIT  ( STEREO )
#define STEREO_16BIT ( STEREO | SIXTEEN_BIT )

int   BLASTER_GetMidiVolume( void );
void  BLASTER_SetMidiVolume( int volume );
int   BLASTER_CardHasMixer( void );
void  BLASTER_SaveMidiVolume( void );
void  BLASTER_RestoreMidiVolume( void );
int   BLASTER_SetupWaveBlaster( int Address );
void  BLASTER_ShutdownWaveBlaster( void );
void  BLASTER_Shutdown( void );

#endif
