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

extern BLASTER_CONFIG BLASTER_Config;
extern int BLASTER_DMAChannel;

#define UNDEFINED -1

enum BLASTER_ERRORS
   {
   BLASTER_Warning = -2,
   BLASTER_Error = -1,
   BLASTER_Ok = 0,
   BLASTER_EnvNotFound,
   BLASTER_AddrNotSet,
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960116L)
   BLASTER_IntNotSet,
#endif
   BLASTER_DMANotSet,
   BLASTER_DMA16NotSet,
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960116L)
   BLASTER_MIDINotSet,
   BLASTER_CardTypeNotSet,
#if (LIBVER_ASSREV < 19950821L)
   BLASTER_UnsupportedSBCardType,
#endif
#endif
   BLASTER_InvalidParameter,
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960116L)
   BLASTER_UnsupportedCardType,
#endif
   BLASTER_CardNotReady,
   BLASTER_NoSoundPlaying,
   BLASTER_InvalidIrq,
   BLASTER_UnableToSetIrq,
   BLASTER_DmaError,
   BLASTER_NoMixer,
   BLASTER_DPMI_Error,
   BLASTER_OutOfMemory
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
#define MONO_16BIT   ( SIXTEEN_BIT )
#define STEREO_16BIT ( STEREO | SIXTEEN_BIT )

#define BLASTER_MaxMixMode        STEREO_16BIT

#define MONO_8BIT_SAMPLE_SIZE    1
#define MONO_16BIT_SAMPLE_SIZE   2
#define STEREO_8BIT_SAMPLE_SIZE  ( 2 * MONO_8BIT_SAMPLE_SIZE )
#define STEREO_16BIT_SAMPLE_SIZE ( 2 * MONO_16BIT_SAMPLE_SIZE )

#define BLASTER_DefaultSampleRate 11000
#define BLASTER_DefaultMixMode    MONO_8BIT
#define BLASTER_MaxIrq            15

unsigned BLASTER_GetPlaybackRate( void );
int   BLASTER_SetMixMode( int mode );
void  BLASTER_StopPlayback( void );
int   BLASTER_GetCurrentPos( void );
int   BLASTER_BeginBufferedPlayback( char *BufferStart,
         int BufferSize, int NumDivisions, unsigned SampleRate,
         int MixMode, void ( *CallBackFunc )( void ) );
int   BLASTER_SetVoiceVolume( int volume );
int   BLASTER_GetMidiVolume( void );
void   BLASTER_SetMidiVolume( int volume );
int   BLASTER_CardHasMixer( void );
void  BLASTER_SaveMidiVolume( void );
void  BLASTER_RestoreMidiVolume( void );
int   BLASTER_GetEnv( BLASTER_CONFIG *Config );
int   BLASTER_SetCardSettings( BLASTER_CONFIG Config );
int   BLASTER_GetCardSettings( BLASTER_CONFIG *Config );
int   BLASTER_GetCardInfo( int *MaxSampleBits, int *MaxChannels );
// *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960116L)
int   BLASTER_SetupWaveBlaster( int Address );
#else
void  BLASTER_SetupWaveBlaster( void );
#endif
void  BLASTER_ShutdownWaveBlaster( void );
int   BLASTER_Init( void );
void  BLASTER_Shutdown( void );

#endif
