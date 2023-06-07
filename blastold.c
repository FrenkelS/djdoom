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
   module: BLASTER.C

   author: James R. Dose
   date:   February 4, 1994

   Low level routines to support Sound Blaster, Sound Blaster Pro,
   Sound Blaster 16, and compatible sound cards.

   (c) Copyright 1994 James R. Dose.  All Rights Reserved.
**********************************************************************/

#include <dos.h>
#include <conio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
//#include "dpmi.h"
//#include "dma.h"
//#include "irq.h"
#include "blaster.h"

#define VALID   ( 1 == 1 )
#define INVALID ( !VALID )

#define TRUE  ( 1 == 1 )
#define FALSE ( !TRUE )

#define YES ( 1 == 1 )
#define NO  ( !YES )

#define lobyte( num )   ( ( int )*( ( char * )&( num ) ) )
#define hibyte( num )   ( ( int )*( ( ( char * )&( num ) ) + 1 ) )

#define BLASTER_MixerAddressPort  0x04
#define BLASTER_MixerDataPort     0x05
#define BLASTER_ResetPort         0x06
#define BLASTER_ReadPort          0x0A
#define BLASTER_WritePort         0x0C
#define BLASTER_DataAvailablePort 0x0E
#define BLASTER_Ready             0xAA
#define BLASTER_16BitDMAAck       0x0F

#define MIXER_DSP4xxISR_Ack       0x82
#define MIXER_DSP4xxISR_Enable    0x83
#define MIXER_MPU401_INT          0x4
#define MIXER_16BITDMA_INT        0x2
#define MIXER_8BITDMA_INT         0x1
#define MIXER_DisableMPU401Interrupts 0xB
#define MIXER_SBProOutputSetting  0x0E
#define MIXER_SBProStereoFlag     0x02
#define MIXER_SBProVoice          0x04
#define MIXER_SBProMidi           0x26
#define MIXER_SB16VoiceLeft       0x32
#define MIXER_SB16VoiceRight      0x33
#define MIXER_SB16MidiLeft        0x34
#define MIXER_SB16MidiRight       0x35

#define DSP_Version1xx            0x0100
#define DSP_Version2xx            0x0200
#define DSP_Version201            0x0201
#define DSP_Version3xx            0x0300
#define DSP_Version4xx            0x0400
#define DSP_SB16Version           DSP_Version4xx

#define DSP_MaxNormalRate         22000
#define DSP_MaxHighSpeedRate      44000

#define DSP_8BitAutoInitRecord        0x2c
#define DSP_8BitHighSpeedAutoInitRecord 0x98
#define DSP_Old8BitADC                0x24
#define DSP_8BitAutoInitMode          0x1c
#define DSP_8BitHighSpeedAutoInitMode 0x90
#define DSP_SetBlockLength            0x48
#define DSP_Old8BitDAC                0x14
#define DSP_16BitDAC                  0xB6
#define DSP_8BitDAC                   0xC6
#define DSP_8BitADC                   0xCe
#define DSP_SetTimeConstant           0x40
#define DSP_Set_DA_Rate               0x41
#define DSP_Set_AD_Rate               0x42
#define DSP_Halt8bitTransfer          0xd0
#define DSP_Continue8bitTransfer      0xd4
#define DSP_Halt16bitTransfer         0xd5
#define DSP_Continue16bitTransfer     0xd6
#define DSP_SpeakerOn                 0xd1
#define DSP_SpeakerOff                0xd3
#define DSP_GetVersion                0xE1
#define DSP_Reset                     0xFFFF

#define DSP_SignedBit                 0x10
#define DSP_StereoBit                 0x20

#define DSP_UnsignedMonoData      0x00
#define DSP_SignedMonoData        ( DSP_SignedBit )
#define DSP_UnsignedStereoData    ( DSP_StereoBit )
#define DSP_SignedStereoData      ( DSP_SignedBit | DSP_StereoBit )

#define BlasterEnv_Address    'A'
#define BlasterEnv_Interrupt  'I'
#define BlasterEnv_8bitDma    'D'
#define BlasterEnv_16bitDma   'H'
#define BlasterEnv_Type       'T'
#define BlasterEnv_Midi       'P'
#define BlasterEnv_EmuAddress 'E'

#define CalcTimeConstant( rate, samplesize ) \
   ( ( 65536L - ( 256000000L / ( ( samplesize ) * ( rate ) ) ) ) >> 8 )

#define CalcSamplingRate( tc ) \
   ( 256000000L / ( 65536L - ( tc << 8 ) ) )

typedef struct
   {
   int IsSupported;
   int HasMixer;
   int MaxMixMode;
   int MinSamplingRate;
   int MaxSamplingRate;
   } CARD_CAPABILITY;

#if (LIBVER_ASSREV < 20021225L) // *** VERSIONS RESTORATION ***
//#include "memcheck.h"
#endif

#define USESTACK

const int BLASTER_Interrupts[ BLASTER_MaxIrq + 1 ]  =
   {
   INVALID, INVALID,     0xa,     0xb,
   INVALID,     0xd, INVALID,     0xf,
   INVALID, INVALID,    0x72,    0x73,
      0x74, INVALID, INVALID,    0x77
   };

const int BLASTER_SampleSize[ BLASTER_MaxMixMode + 1 ] =
   {
   MONO_8BIT_SAMPLE_SIZE,  STEREO_8BIT_SAMPLE_SIZE,
   MONO_16BIT_SAMPLE_SIZE, STEREO_16BIT_SAMPLE_SIZE
   };

const CARD_CAPABILITY BLASTER_CardConfig[ BLASTER_MaxCardType + 1 ] =
   {
      { FALSE, INVALID,      INVALID, INVALID, INVALID }, // Unsupported
      {  TRUE,      NO,    MONO_8BIT,    4000,   23000 }, // SB 1.0
      {  TRUE,     YES,  STEREO_8BIT,    4000,   44100 }, // SBPro
      {  TRUE,      NO,    MONO_8BIT,    4000,   23000 }, // SB 2.xx
      {  TRUE,     YES,  STEREO_8BIT,    4000,   44100 }, // SBPro 2
      { FALSE, INVALID,      INVALID, INVALID, INVALID }, // Unsupported
      {  TRUE,     YES, STEREO_16BIT,    5000,   44100 }, // SB16
   };

BLASTER_CONFIG BLASTER_Config =
   {
   UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED
   };

volatile int   BLASTER_SoundPlaying;
volatile int   BLASTER_SoundRecording;

void ( *BLASTER_CallBack )( void );

static int BLASTER_MixerAddress = UNDEFINED;
static int BLASTER_MixerType    = 0;
static int BLASTER_OriginalMidiVolumeLeft   = 255;
static int BLASTER_OriginalMidiVolumeRight  = 255;

static int BLASTER_WaveBlasterPort  = UNDEFINED;
static int BLASTER_WaveBlasterState = 0x0F;

// adequate stack size
#define kStackSize 2048

// These declarations are necessary to use the inline assembly pragmas.

extern void GetStack(unsigned short *selptr,unsigned long *stackptr);
extern void SetStack(unsigned short selector,unsigned long stackptr);

// This function will get the current stack selector and pointer and save
// them off.
#pragma aux GetStack =  \
   "mov  [edi],esp"     \
   "mov  ax,ss"         \
   "mov  [esi],ax"      \
   parm [esi] [edi]     \
   modify [eax esi edi];

// This function will set the stack selector and pointer to the specified
// values.
#pragma aux SetStack =  \
   "mov  ss,ax"         \
   "mov  esp,edx"       \
   parm [ax] [edx]      \
   modify [eax edx];

int BLASTER_DMAChannel;

int BLASTER_ErrorCode = BLASTER_Ok;

#define BLASTER_SetErrorCode( status ) \
   BLASTER_ErrorCode   = ( status );

static int   BLASTER_GetEnv( BLASTER_CONFIG *Config );

/*---------------------------------------------------------------------
   Function: BLASTER_WriteMixer

   Writes a byte of data to the Sound Blaster's mixer chip.
---------------------------------------------------------------------*/

static void BLASTER_WriteMixer
   (
   int reg,
   int data
   )

   {
   outp( BLASTER_MixerAddress + BLASTER_MixerAddressPort, reg );
   outp( BLASTER_MixerAddress + BLASTER_MixerDataPort, data );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_ReadMixer

   Reads a byte of data from the Sound Blaster's mixer chip.
---------------------------------------------------------------------*/

static int BLASTER_ReadMixer
   (
   int reg
   )

   {
   int data;

   outp( BLASTER_MixerAddress + BLASTER_MixerAddressPort, reg );
   data = inp( BLASTER_MixerAddress + BLASTER_MixerDataPort );
   return( data );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_GetMidiVolume

   Reads the average volume of the Midi sound channel from the
   Sound Blaster's mixer chip.
---------------------------------------------------------------------*/

int BLASTER_GetMidiVolume
   (
   void
   )

   {
   int volume;
   int left;
   int right;

   switch( BLASTER_MixerType )
      {
      case SBPro :
      case SBPro2 :
         left   = BLASTER_ReadMixer( MIXER_SBProMidi );
         right  = ( left & 0x0f ) << 4;
         left  &= 0xf0;
         volume = ( left + right ) / 2;
         break;

      case SB16 :
         left  = BLASTER_ReadMixer( MIXER_SB16MidiLeft );
         right = BLASTER_ReadMixer( MIXER_SB16MidiRight );
         volume = ( left + right ) / 2;
         break;

      default :
         BLASTER_SetErrorCode( BLASTER_NoMixer );
         volume = BLASTER_Error;
      }

   return( volume );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_SetMidiVolume

   Sets the volume of the Midi sound channel on the Sound
   Blaster's mixer chip.
---------------------------------------------------------------------*/

void BLASTER_SetMidiVolume
   (
   int volume
   )

   {
   int data;

   volume = min( 255, volume );
   volume = max( 0, volume );

   switch( BLASTER_MixerType )
      {
      case SBPro :
      case SBPro2 :
         data = ( volume & 0xf0 ) + ( volume >> 4 );
         BLASTER_WriteMixer( MIXER_SBProMidi, data );
         break;

      case SB16 :
         BLASTER_WriteMixer( MIXER_SB16MidiLeft, volume & 0xf8 );
         BLASTER_WriteMixer( MIXER_SB16MidiRight, volume & 0xf8 );
         break;

      default :
         BLASTER_SetErrorCode( BLASTER_NoMixer );
      }

   }

/*---------------------------------------------------------------------
   Function: BLASTER_CardHasMixer

   Checks if the selected Sound Blaster card has a mixer.
---------------------------------------------------------------------*/

int BLASTER_CardHasMixer
   (
   void
   )

   {
   BLASTER_CONFIG Blaster;
   int status;

   if ( BLASTER_MixerAddress == UNDEFINED )
      {
      status = BLASTER_GetEnv( &Blaster );
      if ( status == BLASTER_Ok )
         {
         BLASTER_MixerAddress = Blaster.Address;
         BLASTER_MixerType = 0;
         // *** VERSIONS RESTORATION ***
         // A bug that got fixed
#if (LIBVER_ASSREV < 19950821L)
         if ( ( Blaster.Type >= BLASTER_MinCardType ) ||
            ( Blaster.Type <= BLASTER_MaxCardType ) )
#else
         if ( ( Blaster.Type < BLASTER_MinCardType ) ||
            ( Blaster.Type > BLASTER_MaxCardType ) )
#endif
            {
            BLASTER_MixerType = Blaster.Type;
            }
         }
      }

   if ( BLASTER_MixerAddress != UNDEFINED )
      {
      return( BLASTER_CardConfig[ BLASTER_MixerType ].HasMixer );
      }

   return( FALSE );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_SaveMidiVolume

   Saves the user's FM mixer settings.
---------------------------------------------------------------------*/

void BLASTER_SaveMidiVolume
   (
   void
   )

   {
   if ( BLASTER_CardHasMixer() )
      {
      switch( BLASTER_MixerType )
         {
         case SBPro :
         case SBPro2 :
            BLASTER_OriginalMidiVolumeLeft =
               BLASTER_ReadMixer( MIXER_SBProMidi );
            break;

         case SB16 :
            BLASTER_OriginalMidiVolumeLeft =
               BLASTER_ReadMixer( MIXER_SB16MidiLeft );
            BLASTER_OriginalMidiVolumeRight =
               BLASTER_ReadMixer( MIXER_SB16MidiRight );
            break;
         }
      }
   }


/*---------------------------------------------------------------------
   Function: BLASTER_RestoreMidiVolume

   Restores the user's FM mixer settings.
---------------------------------------------------------------------*/

void BLASTER_RestoreMidiVolume
   (
   void
   )

   {
   if ( BLASTER_CardHasMixer() )
      {
      switch( BLASTER_MixerType )
         {
         case SBPro :
         case SBPro2 :
            BLASTER_WriteMixer( MIXER_SBProMidi,
               BLASTER_OriginalMidiVolumeLeft );
            break;

         case SB16 :
            BLASTER_WriteMixer( MIXER_SB16MidiLeft,
               BLASTER_OriginalMidiVolumeLeft );
            BLASTER_WriteMixer( MIXER_SB16MidiRight,
               BLASTER_OriginalMidiVolumeRight );
            break;
         }
      }
   }


/*---------------------------------------------------------------------
   Function: BLASTER_GetEnv

   Retrieves the BLASTER environment settings and returns them to
   the caller.
---------------------------------------------------------------------*/

static int BLASTER_GetEnv
   (
   BLASTER_CONFIG *Config
   )

   {
   char *Blaster;
   char parameter;
   int  status;
   int  errorcode;

   Config->Address   = UNDEFINED;
   Config->Type      = UNDEFINED;
   Config->Interrupt = UNDEFINED;
   Config->Dma8      = UNDEFINED;
   Config->Dma16     = UNDEFINED;
   Config->Midi      = UNDEFINED;
   Config->Emu       = UNDEFINED;

   Blaster = getenv( "BLASTER" );
   if ( Blaster == NULL )
      {
      BLASTER_SetErrorCode( BLASTER_EnvNotFound );
      return( BLASTER_Error );
      }

   while( *Blaster != 0 )
      {
      if ( *Blaster == ' ' )
         {
         Blaster++;
         continue;
         }

      parameter = toupper( *Blaster );
      Blaster++;

      if ( !isxdigit( *Blaster ) )
         {
         BLASTER_SetErrorCode( BLASTER_InvalidParameter );
         return( BLASTER_Error );
         }

      switch( parameter )
         {
         case BlasterEnv_Address :
            sscanf( Blaster, "%x", &Config->Address );
            break;
         case BlasterEnv_Interrupt :
            sscanf( Blaster, "%d", &Config->Interrupt );
            break;
         case BlasterEnv_8bitDma :
            sscanf( Blaster, "%d", &Config->Dma8 );
            break;
         case BlasterEnv_Type :
            sscanf( Blaster, "%d", &Config->Type );
            break;
         case BlasterEnv_16bitDma :
            sscanf( Blaster, "%d", &Config->Dma16 );
            break;
         case BlasterEnv_Midi :
            sscanf( Blaster, "%x", &Config->Midi );
            break;
         case BlasterEnv_EmuAddress :
            sscanf( Blaster, "%x", &Config->Emu );
            break;
         default  :
            // Skip the offending data
            // sscanf( Blaster, "%*s" );
            break;
         }

      while( isxdigit( *Blaster ) )
         {
         Blaster++;
         }
      }

   status    = BLASTER_Ok;
   errorcode = BLASTER_Ok;

   if ( Config->Type == UNDEFINED )
      {
      errorcode = BLASTER_CardTypeNotSet;
      }
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   else if ( ( Config->Type < BLASTER_MinCardType ) ||
      ( Config->Type > BLASTER_MaxCardType ) ||
      ( !BLASTER_CardConfig[ Config->Type ].IsSupported ) )
      {
      errorcode = BLASTER_UnsupportedCardType;
      }
#endif

   if ( Config->Dma8 == UNDEFINED )
      {
      errorcode = BLASTER_DMANotSet;
      }

   if ( Config->Interrupt == UNDEFINED )
      {
      errorcode = BLASTER_IntNotSet;
      }

   if ( Config->Address == UNDEFINED )
      {
      errorcode = BLASTER_AddrNotSet;
      }

   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19950821L)
   if ( ( Config->Type < BLASTER_MinCardType ) ||
      ( Config->Type > BLASTER_MaxCardType ) ||
      ( !BLASTER_CardConfig[ Config->Type ].IsSupported ) )
      {
      errorcode = BLASTER_UnsupportedCardType;
      }

#endif
   if ( errorcode != BLASTER_Ok )
      {
      status = BLASTER_Error;
      BLASTER_SetErrorCode( errorcode );
      }

   return( status );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_SetupWaveBlaster

   Allows the WaveBlaster to play music while the Sound Blaster 16
   plays digital sound.
---------------------------------------------------------------------*/

int BLASTER_SetupWaveBlaster
   (
   int address
   )

   {
   BLASTER_CONFIG Blaster;
   int status;

   if ( address != UNDEFINED )
      {
      BLASTER_WaveBlasterPort = address;
      }
   else
      {
      if ( BLASTER_Config.Midi == UNDEFINED )
         {
         status = BLASTER_GetEnv( &Blaster );
         if ( status == BLASTER_Ok )
            {
            if ( Blaster.Midi == UNDEFINED )
               {
               BLASTER_SetErrorCode( BLASTER_MIDINotSet );
               return( BLASTER_Error );
               }
            BLASTER_WaveBlasterPort = Blaster.Midi;
            }
         else
            {
            return( status );
            }
         }
      }

   if ( BLASTER_CardHasMixer() )
      {
      // Disable MPU401 interrupts.  If they are not disabled,
      // the SB16 will not produce sound or music.
      BLASTER_WaveBlasterState = BLASTER_ReadMixer( MIXER_DSP4xxISR_Enable );
      BLASTER_WriteMixer( MIXER_DSP4xxISR_Enable, MIXER_DisableMPU401Interrupts );
      }

   return( BLASTER_WaveBlasterPort );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_ShutdownWaveBlaster

   Restores WaveBlaster mixer to original state.
---------------------------------------------------------------------*/

void BLASTER_ShutdownWaveBlaster
   (
   void
   )

   {
   if ( BLASTER_CardHasMixer() )
      {
      // Restore the state of MPU401 interrupts.  If they are not disabled,
      // the SB16 will not produce sound or music.
      BLASTER_WriteMixer( MIXER_DSP4xxISR_Enable, BLASTER_WaveBlasterState );
      }
   }
