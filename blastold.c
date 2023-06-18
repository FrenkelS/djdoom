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

static int BLASTER_MixerAddress = UNDEFINED;
static int BLASTER_MixerType    = 0;

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
   Function: BLASTER_SetVoiceVolume

   Sets the volume of the digitized sound channel on the Sound
   Blaster's mixer chip.
---------------------------------------------------------------------*/

void BLASTER_SetVoiceVolume
   (
   int volume
   )

   {
   int data;
   int status;

   volume = min( 255, volume );
   volume = max( 0, volume );

   status = BLASTER_Ok;
   switch( BLASTER_MixerType )
      {
      case SBPro :
      case SBPro2 :
         data = ( volume & 0xf0 ) + ( volume >> 4 );
         BLASTER_WriteMixer( MIXER_SBProVoice, data );
         break;

      case SB16 :
         BLASTER_WriteMixer( MIXER_SB16VoiceLeft, volume & 0xf8 );
         BLASTER_WriteMixer( MIXER_SB16VoiceRight, volume & 0xf8 );
         break;

      default :
         status = BLASTER_Error;
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
   Function: BLASTER_GetEnv

   Retrieves the BLASTER environment settings and returns them to
   the caller.
---------------------------------------------------------------------*/

int BLASTER_GetEnv
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
   Function: BLASTER_SetCardSettings

   Sets up the sound card's parameters.
---------------------------------------------------------------------*/

void BLASTER_SetCardSettings
   (
   BLASTER_CONFIG Config
   )

   {
   if ( BLASTER_Installed )
      {
      BLASTER_Shutdown();
      }

   if ( ( Config.Type < BLASTER_MinCardType ) ||
      ( Config.Type > BLASTER_MaxCardType ) ||
      ( !BLASTER_CardConfig[ Config.Type ].IsSupported ) )
      {
      return;
      }

   BLASTER_Config.Address   = Config.Address;
   BLASTER_Config.Type      = Config.Type;
   BLASTER_Config.Interrupt = Config.Interrupt;
   BLASTER_Config.Dma8      = Config.Dma8;
   BLASTER_Config.Dma16     = Config.Dma16;
   BLASTER_Config.Midi      = Config.Midi;
   BLASTER_Config.Emu       = Config.Emu;

   BLASTER_MixerAddress = Config.Address;
   BLASTER_MixerType = 0;
   BLASTER_MixerType = Config.Type;
   }


/*---------------------------------------------------------------------
   Function: BLASTER_GetCardInfo

   Returns the maximum number of bits that can represent a sample
   (8 or 16) and the number of channels (1 for mono, 2 for stereo).
---------------------------------------------------------------------*/

void BLASTER_GetCardInfo
   (
   int *MaxSampleBits,
   int *MaxChannels
   )

   {
   int CardType;

   CardType = BLASTER_Config.Type;

   if ( CardType == UNDEFINED )
      {
      return;
      }

   if ( BLASTER_CardConfig[ CardType ].MaxMixMode & STEREO )
      {
      *MaxChannels = 2;
      }
   else
      {
      *MaxChannels = 1;
      }

   if ( BLASTER_CardConfig[ CardType ].MaxMixMode & SIXTEEN_BIT )
      {
      *MaxSampleBits = 16;
      }
   else
      {
      *MaxSampleBits = 8;
      }
   }


/*---------------------------------------------------------------------
   Function: BLASTER_SetupWaveBlaster

   Allows the WaveBlaster to play music while the Sound Blaster 16
   plays digital sound.
---------------------------------------------------------------------*/

void BLASTER_SetupWaveBlaster
   (
   void
   )

   {
   if ( BLASTER_CardHasMixer() )
      {
      // Disable MPU401 interrupts.  If they are not disabled,
      // the SB16 will not produce sound or music.
      BLASTER_WriteMixer( MIXER_DSP4xxISR_Enable, MIXER_DisableMPU401Interrupts );
      }
   }


/*---------------------------------------------------------------------
   Function: BLASTER_Init

   Initializes the sound card and prepares the module to play
   digitized sounds.
---------------------------------------------------------------------*/

int BLASTER_Init
   (
   void
   )

   {
   int Irq;
   int Interrupt;
   int status;

   if ( BLASTER_Installed )
      {
      BLASTER_Shutdown();
      }

   // Save the interrupt masks
   BLASTER_IntController1Mask = inp( 0x21 );
   BLASTER_IntController2Mask = inp( 0xA1 );

   status = BLASTER_ResetDSP();
   if ( status == BLASTER_Ok )
      {
      BLASTER_SaveVoiceVolume();

      BLASTER_SoundPlaying = FALSE;

      BLASTER_SetCallBack( NULL );

      BLASTER_DMABuffer = NULL;

      BLASTER_Version = BLASTER_GetDSPVersion();

      BLASTER_SetPlaybackRate( BLASTER_DefaultSampleRate );
      BLASTER_SetMixMode( BLASTER_DefaultMixMode );

      if ( BLASTER_Config.Dma16 != UNDEFINED )
         {
         status = DMA_VerifyChannel( BLASTER_Config.Dma16 );
         if ( status == DMA_Error )
            {
            return( BLASTER_Error );
            }
         }

      if ( BLASTER_Config.Dma8 != UNDEFINED )
         {
         status = DMA_VerifyChannel( BLASTER_Config.Dma8 );
         if ( status == DMA_Error )
            {
            return( BLASTER_Error );
            }
         }

      // Install our interrupt handler
      Irq = BLASTER_Config.Interrupt;
      if ( !VALID_IRQ( Irq ) )
         {
         return( BLASTER_Error );
         }

      Interrupt = BLASTER_Interrupts[ Irq ];
      if ( Interrupt == INVALID )
         {
         return( BLASTER_Error );
         }

      status = BLASTER_LockMemory();
      if ( status != BLASTER_Ok )
         {
         BLASTER_UnlockMemory();
         return( status );
         }

      StackSelector = allocateTimerStack( kStackSize );
      if ( StackSelector == NULL )
         {
         BLASTER_UnlockMemory();
         return( BLASTER_Error );
         }

      // Leave a little room at top of stack just for the hell of it...
      StackPointer = kStackSize - sizeof( long );

      BLASTER_OldInt = _dos_getvect( Interrupt );
      if ( Irq < 8 )
         {
         _dos_setvect( Interrupt, BLASTER_ServiceInterrupt );
         }
      else
         {
         status = IRQ_SetVector( Interrupt, BLASTER_ServiceInterrupt );
         if ( status != IRQ_Ok )
            {
            BLASTER_UnlockMemory();
            deallocateTimerStack( StackSelector );
            StackSelector = NULL;
            return( BLASTER_Error );
            }
         }

      BLASTER_Installed = TRUE;
      status = BLASTER_Ok;
      }

   return( status );
   }
