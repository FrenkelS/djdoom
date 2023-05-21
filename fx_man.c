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
#include "multivoc.h"
#include "blaster.h"
#include "pas16.h"
#include "sndscape.h"
#include "guswave.h"
#include "sndsrc.h"
// *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960116L)
#include "adlibfx.h"
#include "pcfx.h"
#endif
#include "ll_man.h"
#include "user.h"
#include "fx_man.h"

#define TRUE  ( 1 == 1 )
#define FALSE ( !TRUE )

static unsigned FX_MixRate;

static int FX_SoundDevice = -1;
static int FX_ErrorCode = FX_Ok;
static int FX_Installed = FALSE;

#define FX_SetErrorCode( status ) \
   FX_ErrorCode = ( status );

/*---------------------------------------------------------------------
   Function: FX_GetBlasterSettings

   Returns the current BLASTER environment variable settings.
---------------------------------------------------------------------*/

int FX_GetBlasterSettings
   (
   fx_blaster_config *blaster
   )

   {
   int status;
   BLASTER_CONFIG Blaster;

   FX_SetErrorCode( FX_Ok );

   status = BLASTER_GetEnv( &Blaster );
   if ( status != BLASTER_Ok )
      {
      FX_SetErrorCode( FX_BlasterError );
      return( FX_Error );
      }

   blaster->Type      = Blaster.Type;
   blaster->Address   = Blaster.Address;
   blaster->Interrupt = Blaster.Interrupt;
   blaster->Dma8      = Blaster.Dma8;
   blaster->Dma16     = Blaster.Dma16;
   blaster->Midi      = Blaster.Midi;
   blaster->Emu       = Blaster.Emu;

   return( FX_Ok );
   }


/*---------------------------------------------------------------------
   Function: FX_SetupSoundBlaster

   Handles manual setup of the Sound Blaster information.
---------------------------------------------------------------------*/

int FX_SetupSoundBlaster
   (
   fx_blaster_config blaster,
   int *MaxVoices,
   int *MaxSampleBits,
   int *MaxChannels
   )

   {
   int DeviceStatus;
   BLASTER_CONFIG Blaster;

   FX_SetErrorCode( FX_Ok );

   FX_SoundDevice = SoundBlaster;

   Blaster.Type      = blaster.Type;
   Blaster.Address   = blaster.Address;
   Blaster.Interrupt = blaster.Interrupt;
   Blaster.Dma8      = blaster.Dma8;
   Blaster.Dma16     = blaster.Dma16;
   Blaster.Midi      = blaster.Midi;
   Blaster.Emu       = blaster.Emu;

   BLASTER_SetCardSettings( Blaster );

   DeviceStatus = BLASTER_Init();
   if ( DeviceStatus != BLASTER_Ok )
      {
      FX_SetErrorCode( FX_SoundCardError );
      return( FX_Error );
      }

   *MaxVoices = 8;
   BLASTER_GetCardInfo( MaxSampleBits, MaxChannels );

   return( FX_Ok );
   }


/*---------------------------------------------------------------------
   Function: FX_Init

   Selects which sound device to use.
---------------------------------------------------------------------*/

int FX_Init
   (
   int SoundCard,
   int numvoices,
   int numchannels,
   int samplebits,
   unsigned mixrate
   )

   {
   int status;
   int devicestatus;

   // *** VERSIONS RESTORATION ***
   // FIXME - Can't be a vanilla bug?
#if (LIBVER_ASSREV < 19950821L)
   if ( status = FX_ErrorCode )
      {
      return( FX_Error );
      }
#else
   if ( FX_Installed )
      {
      FX_Shutdown();
      }
#endif

   if ( USER_CheckParameter( "ASSVER" ) )
      {
      FX_SetErrorCode( FX_ASSVersion );
      return( FX_Error );
      }

   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19950821L)
   if ( LL_LockMemory() != LL_Ok )
#else
   status = LL_LockMemory();
   if ( status != LL_Ok )
#endif
      {
      FX_SetErrorCode( FX_DPMI_Error );
      return( FX_Error );
      }

   FX_MixRate = mixrate;

   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   status = FX_Ok;
#endif
   FX_SoundDevice = SoundCard;
   switch( SoundCard )
      {
      case SoundBlaster :
      case Awe32 :
      case ProAudioSpectrum :
      case SoundMan16 :
      case SoundScape :
      case SoundSource :
      case TandySoundSource :
      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
      case UltraSound :
#endif
         devicestatus = MV_Init( SoundCard, FX_MixRate, numvoices,
            numchannels, samplebits );
         if ( devicestatus != MV_Ok )
            {
            FX_SetErrorCode( FX_MultiVocError );
            status = FX_Error;
            }
         break;

      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960116L)
#if (LIBVER_ASSREV < 19950821L)
      case UltraSound :
         if ( GUSWAVE_Init( numvoices ) != GUSWAVE_Ok )
            {
            FX_SetErrorCode( FX_SoundCardError );
            status = FX_Error;
            }
         break;

#endif
      case Adlib :
         if ( ADLIBFX_Init() != ADLIBFX_Ok )
            {
            FX_SetErrorCode( FX_SoundCardError );
            status = FX_Error;
            }
         break;

      case PC :
         if ( PCFX_Init() != PCFX_Ok )
            {
            FX_SetErrorCode( FX_SoundCardError );
            status = FX_Error;
            }
         break;

#endif // LIBVER_ASSREV
      default :
         FX_SetErrorCode( FX_InvalidCard );
         status = FX_Error;
      }

   if ( status != FX_Ok )
      {
      LL_UnlockMemory();
      }
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   else
      {
      FX_Installed = TRUE;
      }
#endif

   return( status );
   }


/*---------------------------------------------------------------------
   Function: FX_Shutdown

   Terminates use of sound device.
---------------------------------------------------------------------*/

int FX_Shutdown
   (
   void
   )

   {
   int status;

   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   if ( !FX_Installed )
      {
      return( FX_Ok );
      }
#endif

   status = FX_Ok;
   switch( FX_SoundDevice )
      {
      case SoundBlaster :
      case Awe32 :
      case ProAudioSpectrum :
      case SoundMan16 :
      case SoundScape :
      case SoundSource :
      case TandySoundSource :
      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
      case UltraSound :
#endif
         status = MV_Shutdown();
         if ( status != MV_Ok )
            {
            FX_SetErrorCode( FX_MultiVocError );
            status = FX_Error;
            }
         break;

      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960116L)
#if (LIBVER_ASSREV < 19950821L)
      case UltraSound :
         GUSWAVE_Shutdown();
         break;

#endif
      case Adlib :
         ADLIBFX_Shutdown();
         break;

      case PC :
         PCFX_Shutdown();
         break;

#endif // LIBVER_ASSREV
      default :
         FX_SetErrorCode( FX_InvalidCard );
         status = FX_Error;
      }

   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   FX_Installed = FALSE;
#endif
   LL_UnlockMemory();

   return( status );
   }


/*---------------------------------------------------------------------
   Function: FX_SetVolume

   Sets the volume of the current sound device.
---------------------------------------------------------------------*/

void FX_SetVolume
   (
   int volume
   )

   {
   int status;

   switch( FX_SoundDevice )
      {
      case SoundBlaster :
      case Awe32 :
         if ( BLASTER_CardHasMixer() )
            {
            BLASTER_SetVoiceVolume( volume );
            }
         else
            {
            MV_SetVolume( volume );
            }
         break;

      case ProAudioSpectrum :
      case SoundMan16 :
         status = PAS_SetPCMVolume( volume );
         if ( status != PAS_Ok )
            {
            MV_SetVolume( volume );
            }
         break;

      case GenMidi :
      case SoundCanvas :
      case WaveBlaster :
         break;

      case SoundScape :
      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960116L)
      case SoundSource :
      case TandySoundSource :
#endif
         MV_SetVolume( volume );
         break;

      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960116L)
      case Adlib :
         ADLIBFX_SetTotalVolume( volume );
         break;

#endif
      case UltraSound :
         GUSWAVE_SetVolume( volume );
         break;

      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960116L)
      case PC :
         PCFX_SetTotalVolume( volume );
         break;
#else
      case SoundSource :
      case TandySoundSource :
         MV_SetVolume( volume );
         break;
#endif
      }
   }


// *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19960510L)
/*---------------------------------------------------------------------
   Function: FX_EndLooping

   Stops the voice associated with the specified handle from looping
   without stoping the sound.
---------------------------------------------------------------------*/

int FX_EndLooping
   (
   int handle
   )

   {
   int status;

   status = MV_EndLooping( handle );
   if ( status == MV_Error )
      {
      FX_SetErrorCode( FX_MultiVocError );
      status = FX_Warning;
      }

   return( status );
   }
#endif

/*---------------------------------------------------------------------
   Function: FX_SetPan

   Sets the stereo and mono volume level of the voice associated
   with the specified handle.
---------------------------------------------------------------------*/

int FX_SetPan
   (
   int handle,
   int vol,
   int left,
   int right
   )

   {
// *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960116L)
   int status = FX_Ok;

   switch( FX_SoundDevice )
      {
      case SoundBlaster :
      case Awe32 :
      case ProAudioSpectrum :
      case SoundMan16 :
      case SoundScape :
#if (LIBVER_ASSREV >= 19950821L) // VERSIONS RESTORATION
      case UltraSound :
#endif
      case SoundSource :
      case TandySoundSource :
         status = MV_SetPan( handle, vol, left, right );
         if ( status == MV_Error )
            {
            FX_SetErrorCode( FX_MultiVocError );
            status = FX_Warning;
            }
         break;

      case Adlib :
         status = ADLIBFX_SetVolume( handle, vol );
         if ( status == ADLIBFX_Error )
            {
            FX_SetErrorCode( FX_SoundCardError );
            status = FX_Warning;
            }
         break;

#if (LIBVER_ASSREV < 19950821L) // VERSIONS RESTORATION
      case UltraSound :
#endif
      case PC :
         break;

      default:
         FX_SetErrorCode( FX_InvalidCard );
         status = FX_Error;
      }
#else // LIBVER_ASSREV
   int status;

   status = MV_SetPan( handle, vol, left, right );
   if ( status == MV_Error )
      {
      FX_SetErrorCode( FX_MultiVocError );
      status = FX_Warning;
      }
#endif // LIBVER_ASSREV

   return( status );
   }


/*---------------------------------------------------------------------
   Function: FX_SetPitch

   Sets the pitch of the voice associated with the specified handle.
---------------------------------------------------------------------*/

int FX_SetPitch
   (
   int handle,
   int pitchoffset
   )

   {
   int status = FX_Ok;

   switch( FX_SoundDevice )
      {
      case SoundBlaster :
      case Awe32 :
      case ProAudioSpectrum :
      case SoundMan16 :
      case SoundScape :
      case UltraSound :
      case SoundSource :
      case TandySoundSource :
         status = MV_SetPitch( handle, pitchoffset );
         if ( status == MV_Error )
            {
            FX_SetErrorCode( FX_MultiVocError );
            status = FX_Warning;
            }
         break;

      case PC :
         break;

      default :
         FX_SetErrorCode( FX_InvalidCard );
         status = FX_Error;
      }

   return( status );
   }


/*---------------------------------------------------------------------
   Function: FX_PlayRaw

   Begin playback of raw sound data with the given volume and priority.
---------------------------------------------------------------------*/

int FX_PlayRaw
   (
   void *ptr,
   unsigned long length,
   unsigned rate,
   int pitchoffset,
   int vol,
   int left,
   int right,
   int priority,
   unsigned long callbackval
   )

   {
   int handle;

// *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960116L)
   switch( FX_SoundDevice )
      {
      case SoundBlaster :
      case Awe32 :
      case ProAudioSpectrum :
      case SoundMan16 :
      case SoundScape :
#if (LIBVER_ASSREV >= 19950821L) // VERSIONS RESTORATION
      case UltraSound :
#endif
      case SoundSource :
      case TandySoundSource :
         handle = MV_PlayRaw( ptr, length, rate, pitchoffset,
            vol, left, right, priority, callbackval );
         if ( handle < MV_Ok )
            {
            FX_SetErrorCode( FX_MultiVocError );
            handle = FX_Warning;
            }
         break;

      case Adlib :
         // FIXME (RESTORATION) - Might be a vanilla bug here (swapped args)
         handle = ADLIBFX_Play( ptr, priority, vol, callbackval );
         if ( handle < ADLIBFX_Ok )
            {
            FX_SetErrorCode( FX_SoundCardError );
            handle = FX_Warning;
            }
         break;

      case PC :
         handle = PCFX_Play( ptr, priority, callbackval );
         if ( handle < PCFX_Ok )
            {
            FX_SetErrorCode( FX_SoundCardError );
            handle = FX_Warning;
            }
         break;

      default :
         FX_SetErrorCode( FX_InvalidCard );
         handle = FX_Error;
      }
#else // LIBVER_ASSREV
   handle = MV_PlayRaw( ptr, length, rate, pitchoffset,
      vol, left, right, priority, callbackval );
   if ( handle < MV_Ok )
      {
      FX_SetErrorCode( FX_MultiVocError );
      handle = FX_Warning;
      }
#endif // LIBVER_ASSREV

   return( handle );
   }


/*---------------------------------------------------------------------
   Function: FX_SoundActive

   Tests if the specified sound is currently playing.
---------------------------------------------------------------------*/

int FX_SoundActive
   (
   int handle
   )

   {
   switch( FX_SoundDevice )
      {
      case SoundBlaster :
      case Awe32 :
      case ProAudioSpectrum :
      case SoundMan16 :
      case UltraSound :
      case SoundScape :
      case SoundSource :
      case TandySoundSource :
         return( MV_VoicePlaying( handle ) );

      case Adlib :
         return( ADLIBFX_SoundPlaying( handle ) );

      case PC :
         return( PCFX_SoundPlaying( handle ) );
      }
   return( FALSE );
   }


/*---------------------------------------------------------------------
   Function: FX_StopSound

   Halts playback of a specific voice
---------------------------------------------------------------------*/

int FX_StopSound
   (
   int handle
   )

   {
   int status;

   switch( FX_SoundDevice )
      {
      case SoundBlaster :
      case Awe32 :
      case ProAudioSpectrum :
      case SoundMan16 :
      case SoundScape :
      case UltraSound :
      case SoundSource :
      case TandySoundSource :
         status = MV_Kill( handle );
         if ( status != MV_Ok )
            {
            FX_SetErrorCode( FX_MultiVocError );
            return( FX_Warning );
            }
         break;

      case Adlib :
         status = ADLIBFX_Stop( handle );
         if ( status != ADLIBFX_Ok )
            {
            FX_SetErrorCode( FX_SoundCardError );
            return( FX_Warning );
            }
         break;

      case PC :
         status = PCFX_Stop( handle );
         if ( status != PCFX_Ok )
            {
            FX_SetErrorCode( FX_SoundCardError );
            return( FX_Warning );
            }
         break;

      default:
         FX_SetErrorCode( FX_InvalidCard );
       }

   return( FX_Ok );
   }
