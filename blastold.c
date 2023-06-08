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
#include <ctype.h>
#include "blaster.h"
#include "doomdef.h"

#define VALID   ( 1 == 1 )
#define INVALID ( !VALID )

#define TRUE  ( 1 == 1 )
#define FALSE ( !TRUE )

#define YES ( 1 == 1 )
#define NO  ( !YES )

#define BLASTER_MixerAddressPort  0x04
#define BLASTER_MixerDataPort     0x05

#define MIXER_DSP4xxISR_Enable    0x83
#define MIXER_DisableMPU401Interrupts 0xB
#define MIXER_SBProMidi           0x26
#define MIXER_SB16MidiLeft        0x34
#define MIXER_SB16MidiRight       0x35

#define BlasterEnv_Address    'A'
#define BlasterEnv_Interrupt  'I'
#define BlasterEnv_8bitDma    'D'
#define BlasterEnv_16bitDma   'H'
#define BlasterEnv_Type       'T'
#define BlasterEnv_Midi       'P'
#define BlasterEnv_EmuAddress 'E'

typedef struct
   {
   int IsSupported;
   int HasMixer;
   int MaxMixMode;
   int MinSamplingRate;
   int MaxSamplingRate;
   } CARD_CAPABILITY;

static const CARD_CAPABILITY BLASTER_CardConfig[ BLASTER_MaxCardType + 1 ] =
   {
      { FALSE, INVALID,      INVALID, INVALID, INVALID }, // Unsupported
      {  TRUE,      NO,    MONO_8BIT,    4000,   23000 }, // SB 1.0
      {  TRUE,     YES,  STEREO_8BIT,    4000,   44100 }, // SBPro
      {  TRUE,      NO,    MONO_8BIT,    4000,   23000 }, // SB 2.xx
      {  TRUE,     YES,  STEREO_8BIT,    4000,   44100 }, // SBPro 2
      { FALSE, INVALID,      INVALID, INVALID, INVALID }, // Unsupported
      {  TRUE,     YES, STEREO_16BIT,    5000,   44100 }, // SB16
   };

static BLASTER_CONFIG BLASTER_Config =
   {
   UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED
   };

static int BLASTER_MixerAddress = UNDEFINED;
static int BLASTER_MixerType    = 0;
static int BLASTER_OriginalMidiVolumeLeft   = 255;
static int BLASTER_OriginalMidiVolumeRight  = 255;

static int BLASTER_WaveBlasterPort  = UNDEFINED;
static int BLASTER_WaveBlasterState = 0x0F;

static int BLASTER_GetEnv( BLASTER_CONFIG *Config );

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

int32_t BLASTER_GetMidiVolume
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
   int32_t volume
   )

   {
   int data;

   if (volume > 255)
      volume = 255;
   else if (volume < 0)
      volume = 0;

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
         if ( ( Blaster.Type < BLASTER_MinCardType ) ||
            ( Blaster.Type > BLASTER_MaxCardType ) )
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
   else if ( ( Config->Type < BLASTER_MinCardType ) ||
      ( Config->Type > BLASTER_MaxCardType ) ||
      ( !BLASTER_CardConfig[ Config->Type ].IsSupported ) )
      {
      errorcode = BLASTER_UnsupportedCardType;
      }

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

   if ( errorcode != BLASTER_Ok )
      {
      status = BLASTER_Error;
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
