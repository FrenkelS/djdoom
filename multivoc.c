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
   module: MULTIVOC.C

   author: James R. Dose
   date:   December 20, 1993

   Routines to provide multichannel digitized sound playback for
   Sound Blaster compatible sound cards.

   (c) Copyright 1993 James R. Dose.  All Rights Reserved.
**********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <time.h>
#include <conio.h>
#include "dpmi.h"
#include "usrhooks.h"
#if (LIBVER_ASSREV < 20021225L) // *** VERSIONS RESTORATION ***
#include "interrupt.h"
#else
#include "interrup.h"
#endif
#include "dma.h"
// *** VERSIONS RESTORATION ***
// FIXME GUESSING
#if (LIBVER_ASSREV < 19960510L)
#include "ll_man.h"
// FIXME - HUGE HACK
#define LL_Empty(a,b,c) ((a)->start == NULL)
#define LL_Reset( list, next, prev ) (list)->start = NULL; (list)->end = NULL // Based on macro from LINKLIST.H
#else
#include "linklist.h"
#endif
#include "sndcards.h"
#include "blaster.h"
#include "sndscape.h"
#include "sndsrc.h"
#include "pas16.h"
#include "guswave.h"
#include "pitch.h"
#include "multivoc.h"
#include "_multivc.h"
#include "debugio.h"

#define RoundFixed( fixedval, bits )            \
        (                                       \
          (                                     \
            (fixedval) + ( 1 << ( (bits) - 1 ) )\
          ) >> (bits)                           \
        )

#define IS_QUIET( ptr )  ( ( void * )( ptr ) == ( void * )&MV_VolumeTable[ 0 ] )

static int       MV_ReverbLevel;
// *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
static int       MV_ReverbDelay;
#endif
static VOLUME16 *MV_ReverbTable = NULL;

//static signed short MV_VolumeTable[ MV_MaxVolume + 1 ][ 256 ];
static signed short MV_VolumeTable[ 63 + 1 ][ 256 ];

//static Pan MV_PanTable[ MV_NumPanPositions ][ MV_MaxVolume + 1 ];
static Pan MV_PanTable[ MV_NumPanPositions ][ 63 + 1 ];

static int MV_Installed   = FALSE;
static int MV_SoundCard   = SoundBlaster;
static int MV_TotalVolume = MV_MaxTotalVolume;
static int MV_MaxVoices   = 1;
static int MV_Recording;

static int MV_BufferSize = MixBufferSize;
// *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19950821L)
static int MV_SampleSize = 1;
#else
static int MV_BufferLength;
#endif

static int MV_NumberOfBuffers = NumberOfBuffers;

static int MV_MixMode    = MONO_8BIT;
static int MV_Channels   = 1;
static int MV_Bits       = 8;

static int MV_Silence    = SILENCE_8BIT;
static int MV_SwapLeftRight = FALSE;

static int MV_RequestedMixRate;
static int MV_MixRate;

static int MV_DMAChannel = -1;
static int MV_BuffShift;

static int MV_TotalMemory;

static int   MV_BufferDescriptor;
static int   MV_BufferEmpty[ NumberOfBuffers ];
char *MV_MixBuffer[ NumberOfBuffers + 1 ];

static VoiceNode *MV_Voices = NULL;

// *** VERSIONS RESTORATION ***
// 1. Different definitions for list and pool, from MV1.C
// 2. HACK - Misc. convenience macros
// 3. FIXME - Maybe LIBVER_ASS_MV_VOICESTART/LISTEND won't be required (replacing for loops with while loops)
#if (LIBVER_ASSREV < 19960510L)
#define LIBVER_ASS_MV_VOICELISTEND NULL
#define LIBVER_ASS_MV_VOICESTART start
static volatile VList VoiceList = { NULL, NULL };
static volatile VList VoicePool = { NULL, NULL };
#else
#define LIBVER_ASS_MV_VOICELISTEND (&VoiceList)
#define LIBVER_ASS_MV_VOICESTART next
static volatile VoiceNode VoiceList;
static volatile VoiceNode VoicePool;
#endif

static int MV_MixPage      = 0;
static int MV_VoiceHandle  = MV_MinVoiceHandle;

static void ( *MV_CallBackFunc )( unsigned long ) = NULL;
static void ( *MV_RecordFunc )( char *ptr, int length ) = NULL;
static void ( *MV_MixFunction )( VoiceNode *voice, int buffer );

// *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19960510L)
static int MV_MaxVolume = 63;
#endif

char  *MV_HarshClipTable;
char  *MV_MixDestination;
short *MV_LeftVolume;
short *MV_RightVolume;
// *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
int    MV_SampleSize = 1;
#endif
int    MV_RightChannelOffset;

unsigned long MV_MixPosition;

int MV_ErrorCode = MV_Ok;

#define MV_SetErrorCode( status ) \
   MV_ErrorCode   = ( status );


/*---------------------------------------------------------------------
   Function: MV_ErrorString

   Returns a pointer to the error message associated with an error
   number.  A -1 returns a pointer the current error.
---------------------------------------------------------------------*/

char *MV_ErrorString
   (
   int ErrorNumber
   )

   {
   char *ErrorString;

   switch( ErrorNumber )
      {
      case MV_Warning :
      case MV_Error :
         ErrorString = MV_ErrorString( MV_ErrorCode );
         break;

      case MV_Ok :
         ErrorString = "Multivoc ok.";
         break;

      case MV_UnsupportedCard :
         ErrorString = "Selected sound card is not supported by Multivoc.";
         break;

      case MV_NotInstalled :
         ErrorString = "Multivoc not installed.";
         break;

      case MV_NoVoices :
         ErrorString = "No free voices available to Multivoc.";
         break;

      case MV_NoMem :
         ErrorString = "Out of memory in Multivoc.";
         break;

      case MV_VoiceNotFound :
         ErrorString = "No voice with matching handle found.";
         break;

      case MV_BlasterError :
         ErrorString = BLASTER_ErrorString( BLASTER_Error );
         break;

      case MV_PasError :
         ErrorString = PAS_ErrorString( PAS_Error );
         break;

      case MV_SoundScapeError :
         ErrorString = SOUNDSCAPE_ErrorString( SOUNDSCAPE_Error );
         break;

      #ifndef SOUNDSOURCE_OFF
      case MV_SoundSourceError :
         ErrorString = SS_ErrorString( SS_Error );
         break;
      #endif

      case MV_DPMI_Error :
         ErrorString = "DPMI Error in Multivoc.";
         break;

      case MV_InvalidVOCFile :
         ErrorString = "Invalid VOC file passed in to Multivoc.";
         break;

      case MV_InvalidWAVFile :
         ErrorString = "Invalid WAV file passed in to Multivoc.";
         break;

      case MV_InvalidMixMode :
         ErrorString = "Invalid mix mode request in Multivoc.";
         break;

      case MV_SoundSourceFailure :
         ErrorString = "Sound Source playback failed.";
         break;

      case MV_IrqFailure :
         ErrorString = "Playback failed, possibly due to an invalid or conflicting IRQ.";
         break;

      case MV_DMAFailure :
         ErrorString = "Playback failed, possibly due to an invalid or conflicting DMA channel.";
         break;

      case MV_DMA16Failure :
         ErrorString = "Playback failed, possibly due to an invalid or conflicting DMA channel.  \n"
                       "Make sure the 16-bit DMA channel is correct.";
         break;

      case MV_NullRecordFunction :
         ErrorString = "Null record function passed to MV_StartRecording.";
         break;

      default :
         ErrorString = "Unknown Multivoc error code.";
         break;
      }

   return( ErrorString );
   }


/**********************************************************************

   Memory locked functions:

**********************************************************************/


#define MV_LockStart MV_Mix


/*---------------------------------------------------------------------
   Function: MV_Mix

   Mixes the sound into the buffer.
---------------------------------------------------------------------*/

static void MV_Mix
   (
   VoiceNode *voice,
   int        buffer
   )

   {
   char          *start;
   int            length;
   long           voclength;
   unsigned long  position;
   unsigned long  rate;
   unsigned long  FixedPointBufferSize;

   if ( ( voice->length == 0 ) && ( voice->GetSound( voice ) != KeepPlaying ) )
      {
      return;
      }

   length               = MixBufferSize;
   FixedPointBufferSize = voice->FixedPointBufferSize;

   MV_MixDestination    = MV_MixBuffer[ buffer ];
   MV_LeftVolume        = voice->LeftVolume;
   MV_RightVolume       = voice->RightVolume;

   if ( ( MV_Channels == 2 ) && ( IS_QUIET( MV_LeftVolume ) ) )
      {
      MV_LeftVolume      = MV_RightVolume;
      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19950821L)
      MV_MixDestination += MV_SampleSize / 2;
#else
      MV_MixDestination += MV_RightChannelOffset;
#endif
      }

   // Add this voice to the mix
   while( length > 0 )
      {
      start    = voice->sound;
      rate     = voice->RateScale;
      position = voice->position;

      // Check if the last sample in this buffer would be
      // beyond the length of the sample block
      if ( ( position + FixedPointBufferSize ) >= voice->length )
         {
         if ( position < voice->length )
            {
            voclength = ( voice->length - position + rate - 1 ) / rate;
            }
         else
            {
            voice->GetSound( voice );
            return;
            }
         }
      else
         {
         voclength = length;
         }

      voice->mix( position, rate, start, voclength );

      if ( voclength & 1 )
         {
         MV_MixPosition += rate;
         voclength -= 1;
         }
      voice->position = MV_MixPosition;

      length -= voclength;

      if ( voice->position >= voice->length )
         {
         // Get the next block of sound
         if ( voice->GetSound( voice ) != KeepPlaying )
            {
            return;
            }

         if ( length > 0 )
            {
            // Get the position of the last sample in the buffer
            FixedPointBufferSize = voice->RateScale * ( length - 1 );
            }
         }
      }
   }


/*---------------------------------------------------------------------
   Function: MV_PlayVoice

   Adds a voice to the play list.
---------------------------------------------------------------------*/

void MV_PlayVoice
   (
   VoiceNode *voice
   )

   {
   unsigned flags;

   flags = DisableInterrupts();
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960510L)
   LL_AddToTail( VoiceNode, &VoiceList, voice );
#else
   LL_SortedInsertion( &VoiceList, voice, prev, next, VoiceNode, priority );
#endif

   RestoreInterrupts( flags );
   }


/*---------------------------------------------------------------------
   Function: MV_StopVoice

   Removes the voice from the play list and adds it to the free list.
---------------------------------------------------------------------*/

void MV_StopVoice
   (
   VoiceNode *voice
   )

   {
   unsigned  flags;

   flags = DisableInterrupts();

   // *** VERSIONS RESTORATION ***

   // move the voice from the play list to the free list
#if (LIBVER_ASSREV < 19960510L)
   LL_Remove( VoiceNode, &VoiceList, voice );
   LL_AddToTail( VoiceNode, &VoicePool, voice );
#else
   LL_Remove( voice, next, prev );
   LL_Add( &VoicePool, voice, next, prev );
#endif

   RestoreInterrupts( flags );
   }

/*---------------------------------------------------------------------
   Function: MV_ServiceVoc

   Starts playback of the waiting buffer and mixes the next one.
---------------------------------------------------------------------*/

// static int backcolor = 1;

void MV_ServiceVoc
   (
   void
   )

   {
   VoiceNode *voice;
   VoiceNode *next;
   char      *buffer;

   if ( MV_DMAChannel >= 0 )
      {
      // Get the currently playing buffer
      buffer = ( char * )DMA_GetCurrentPos( MV_DMAChannel );
      MV_MixPage   = ( unsigned )( buffer - MV_MixBuffer[ 0 ] );
      MV_MixPage >>= MV_BuffShift;
      }

   // Toggle which buffer we'll mix next
   MV_MixPage++;
   // FIXME - VERSIONS RESTORATION TEST - Uncomment to ensure the function
   // body's size is the same as in ROTT Site Licensed v1.3
   //
   // *** THIS MODIFIES THE FUNCTION'S BEHAVIORS ***
#if 0
   MV_MixPage++;
   MV_MixPage++;
#endif
   if ( MV_MixPage >= MV_NumberOfBuffers )
      {
      MV_MixPage -= MV_NumberOfBuffers;
      }

   if ( MV_ReverbLevel == 0 )
      {
      // *** VERSIONS RESTORATION ***
      // Do uncomment this in earlier versions

      // Initialize buffer
      //Commented out so that the buffer is always cleared.
      //This is so the guys at Echo Speech can mix into the
      //buffer even when no sounds are playing.
#if (LIBVER_ASSREV < 19960116L)
      if ( !MV_BufferEmpty[ MV_MixPage ] )
#endif
         {
         ClearBuffer_DW( MV_MixBuffer[ MV_MixPage ], MV_Silence, MV_BufferSize >> 2 );
         // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
         if ( ( MV_SoundCard == UltraSound ) && ( MV_Channels == 2 ) )
            {
            ClearBuffer_DW( MV_MixBuffer[ MV_MixPage ] + MV_RightChannelOffset,
               MV_Silence, MV_BufferSize >> 2 );
            }
#endif
         MV_BufferEmpty[ MV_MixPage ] = TRUE;
         }
      }
   else
      {
      // *** VERSIONS RESTORATION ***
      // FIXME GUESSING, partially based on code from MV1.C (e.g., MV_Mix8bitMono)
#if (LIBVER_ASSREV < 19950821L)
      int   sourceOffset;
      //int   length;
      int   i;
      if ( MV_ReverbTable != NULL )
         {
         if ( ( sourceOffset = MV_MixPage - 3 ) < 0 )
         {
            sourceOffset = MV_MixPage - 3 + MV_NumberOfBuffers;
         }

         if ( MV_Bits == 16 )
            {
            int length = MV_BufferSize / 2;
            MONO16 *to = MV_MixBuffer[MV_MixPage];
            MONO16 *from = MV_MixBuffer[sourceOffset];
            for( i = 0; i < length; i++, from++, to++ )
               {
               *to = *( (MONO16 *)(*MV_ReverbTable) + ((*from + 128) >> 8) + 128);
               }
            }
         else
            {
            unsigned char *from = MV_MixBuffer[sourceOffset];
            unsigned char *to = MV_MixBuffer[MV_MixPage];
            //length = MV_BufferSize;
            for( i = 0; i < MV_BufferSize; i++, from++, to++ )
               {
               *to = *( (unsigned char *)(*MV_ReverbTable) + 2*(*from)) + 128;
               }
            }
         }
      else
         {

         unsigned valToShift = MV_ReverbLevel;
         if ( ( sourceOffset = MV_MixPage - 3 ) < 0 )
         {
            sourceOffset = MV_MixPage - 3 + MV_NumberOfBuffers;
         }

         if ( MV_Bits == 16 )
            {
            MONO16 *from, *to;
            int length = MV_BufferSize / 2;
            /*MONO16 **/from = MV_MixBuffer[sourceOffset];
            /*MONO16 **/to = MV_MixBuffer[MV_MixPage];
            for( i = 0; i < length; i++, from++, to++ )
               {
               *to = (*from) >> (unsigned char)valToShift;
               }
            }
         else
            {
            unsigned char *from, *to;
            int length = MV_BufferSize;
            /*unsigned char **/from = MV_MixBuffer[sourceOffset];
            /*unsigned char **/to = MV_MixBuffer[MV_MixPage];
            for( i = 0; i < length; i++, from++, to++ )
               {
               *to = ((*from) >> (unsigned char)valToShift) + 64;
               }
            }
         }
#else // LIBVER_ASSREV >= 19950821L
      char *end;
      char *source;
      char *dest;
      int   count;
      int   length;

      end = MV_MixBuffer[ 0 ] + MV_BufferLength;;
      dest = MV_MixBuffer[ MV_MixPage ];
      source = MV_MixBuffer[ MV_MixPage ] - MV_ReverbDelay;
      if ( source < MV_MixBuffer[ 0 ] )
         {
         source += MV_BufferLength;
         }

      length = MV_BufferSize;
      while( length > 0 )
         {
         count = length;
         if ( source + count > end )
            {
            count = end - source;
            }

         if ( MV_Bits == 16 )
            {
            if ( MV_ReverbTable != NULL )
               {
               MV_16BitReverb( source, dest, MV_ReverbTable, count / 2 );
               if ( ( MV_SoundCard == UltraSound ) && ( MV_Channels == 2 ) )
                  {
                  MV_16BitReverb( source + MV_RightChannelOffset,
                     dest + MV_RightChannelOffset, MV_ReverbTable, count / 2 );
                  }
               }
            else
               {
               MV_16BitReverbFast( source, dest, count / 2, MV_ReverbLevel );
               if ( ( MV_SoundCard == UltraSound ) && ( MV_Channels == 2 ) )
                  {
                  MV_16BitReverbFast( source + MV_RightChannelOffset,
                     dest + MV_RightChannelOffset, count / 2, MV_ReverbLevel );
                  }
               }
            }
         else
            {
            if ( MV_ReverbTable != NULL )
               {
               MV_8BitReverb( source, dest, MV_ReverbTable, count );
               if ( ( MV_SoundCard == UltraSound ) && ( MV_Channels == 2 ) )
                  {
                  MV_8BitReverb( source + MV_RightChannelOffset,
                     dest + MV_RightChannelOffset, MV_ReverbTable, count );
                  }
               }
            else
               {
               MV_8BitReverbFast( source, dest, count, MV_ReverbLevel );
               if ( ( MV_SoundCard == UltraSound ) && ( MV_Channels == 2 ) )
                  {
                  MV_8BitReverbFast( source + MV_RightChannelOffset,
                     dest + MV_RightChannelOffset, count, MV_ReverbLevel );
                  }
               }
            }

         // if we go through the loop again, it means that we've wrapped around the buffer
         source  = MV_MixBuffer[ 0 ];
         dest   += count;
         length -= count;
         }
#endif // LIBVER_ASSREV < 19950821L
      }

   // *** VERSIONS RESTORATION ***

   // Play any waiting voices
#if (LIBVER_ASSREV < 19960510L)
   voice = VoiceList.start;
   while( voice != NULL )
#else
   for( voice = VoiceList.LIBVER_ASS_MV_VOICESTART; voice != LIBVER_ASS_MV_VOICELISTEND; voice = next )
#endif
      {
//      if ( ( voice < &MV_Voices[ 0 ] ) || ( voice > &MV_Voices[ 8 ] ) )
//         {
//         SetBorderColor(backcolor++);
//         break;
//         }

      MV_BufferEmpty[ MV_MixPage ] = FALSE;

      MV_MixFunction( voice, MV_MixPage );

      next = voice->next;

      // Is this voice done?
      if ( !voice->Playing )
         {
         MV_StopVoice( voice );

         if ( MV_CallBackFunc )
            {
            MV_CallBackFunc( voice->callbackval );
            }
         }
      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960510L)
      voice = next;
#endif
      }
   }


// *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
int leftpage  = -1;
int rightpage = -1;

void MV_ServiceGus( char **ptr, unsigned long *length )
   {
   if ( leftpage == MV_MixPage )
      {
      MV_ServiceVoc();
      }

   leftpage = MV_MixPage;

   *ptr = MV_MixBuffer[ MV_MixPage ];
   *length = MV_BufferSize;
   }

void MV_ServiceRightGus( char **ptr, unsigned long *length )
   {
   if ( rightpage == MV_MixPage )
      {
      MV_ServiceVoc();
      }

   rightpage = MV_MixPage;

   *ptr = MV_MixBuffer[ MV_MixPage ] + MV_RightChannelOffset;
   *length = MV_BufferSize;
   }
#endif // LIBVER_ASSREV >= 19950821L

/*---------------------------------------------------------------------
   Function: MV_GetNextVOCBlock

   Interperate the information of a VOC format sound file.
---------------------------------------------------------------------*/

playbackstatus MV_GetNextVOCBlock
   (
   VoiceNode *voice
   )

   {
   unsigned char *ptr;
   int            blocktype;
   int            lastblocktype;
   unsigned long  blocklength;
   unsigned long  samplespeed;
   unsigned int   tc;
   int            packtype;
   int            voicemode;
   int            done;
   unsigned       BitsPerSample;
   unsigned       Channels;
   unsigned       Format;

   if ( voice->BlockLength > 0 )
      {
      voice->position    -= voice->length;
      voice->sound       += voice->length >> 16;
      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
      if ( voice->bits == 16 )
         {
         voice->sound += voice->length >> 16;
         }
#endif
      voice->length       = min( voice->BlockLength, 0x8000 );
      voice->BlockLength -= voice->length;
      voice->length     <<= 16;
      return( KeepPlaying );
      }

   if ( ( voice->length > 0 ) && ( voice->LoopEnd != NULL ) &&
      ( voice->LoopStart != NULL ) )
      {
      voice->BlockLength  = voice->LoopSize;
      voice->sound        = voice->LoopStart;
      voice->position     = 0;
      voice->length       = min( voice->BlockLength, 0x8000 );
      voice->BlockLength -= voice->length;
      voice->length     <<= 16;
      return( KeepPlaying );
      }

   ptr = ( unsigned char * )voice->NextBlock;

   voice->Playing = TRUE;

   voicemode = 0;
   lastblocktype = 0;
   packtype = 0;

   done = FALSE;
   while( !done )
      {
      // Stop playing if we get a NULL pointer
      if ( ptr == NULL )
         {
         voice->Playing = FALSE;
         done = TRUE;
         break;
         }

      blocktype = ( int )*ptr;
      blocklength = ( *( unsigned long * )( ptr + 1 ) ) & 0x00ffffff;
      ptr += 4;

      switch( blocktype )
         {
         case 0 :
            // End of data
            if ( ( voice->LoopStart == NULL ) ||
               ( voice->LoopStart >= ( ptr - 4 ) ) )
               {
               voice->Playing = FALSE;
               done = TRUE;
               }
            else
               {
               voice->BlockLength  = ( ptr - 4 ) - voice->LoopStart;
               voice->sound        = voice->LoopStart;
               voice->position     = 0;
               voice->length       = min( voice->BlockLength, 0x8000 );
               voice->BlockLength -= voice->length;
               voice->length     <<= 16;
               return( KeepPlaying );
               }
            break;

         case 1 :
            // Sound data block

            // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
            voice->bits  = 8;
#endif
            if ( lastblocktype != 8 )
               {
               tc = ( unsigned int )*ptr << 8;
               packtype = *( ptr + 1 );
               }

            ptr += 2;
            blocklength -= 2;

            samplespeed = 256000000L / ( 65536 - tc );

            // Skip packed or stereo data
            if ( ( packtype != 0 ) || ( voicemode != 0 ) )
               {
               ptr += blocklength;
               }
            else
               {
               done = TRUE;
               }
            voicemode = 0;
            break;

         case 2 :
            // Sound continuation block
            samplespeed = voice->SamplingRate;
            done = TRUE;
            break;

         case 3 :
            // Silence
            // Not implimented.
            ptr += blocklength;
            break;

         case 4 :
            // Marker
            // Not implimented.
            ptr += blocklength;
            break;

         case 5 :
            // ASCII string
            // Not implimented.
            ptr += blocklength;
            break;

         case 6 :
            // Repeat begin
            if ( voice->LoopEnd == NULL )
               {
               voice->LoopCount = *( unsigned short * )ptr;
               voice->LoopStart = ptr + blocklength;
               }
            ptr += blocklength;
            break;

         case 7 :
            // Repeat end
            ptr += blocklength;
            if ( lastblocktype == 6 )
               {
               voice->LoopCount = 0;
               }
            else
               {
               if ( ( voice->LoopCount > 0 ) && ( voice->LoopStart != NULL ) )
                  {
                  ptr = voice->LoopStart;
                  if ( voice->LoopCount < 0xffff )
                     {
                     voice->LoopCount--;
                     if ( voice->LoopCount == 0 )
                        {
                        voice->LoopStart = NULL;
                        }
                     }
                  }
               }
            break;

         case 8 :
            // Extended block

            // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
            voice->bits  = 8;
#endif
            tc = *( unsigned short * )ptr;
            packtype = *( ptr + 2 );
            voicemode = *( ptr + 3 );
            ptr += blocklength;
            break;

         case 9 :
            // New sound data block
            samplespeed = *( unsigned long * )ptr;
            BitsPerSample = ( unsigned )*( ptr + 4 );
            Channels = ( unsigned )*( ptr + 5 );
            Format = ( unsigned )*( unsigned short * )( ptr + 6 );

            if ( ( BitsPerSample == 8 ) && ( Channels == 1 ) &&
               ( Format == VOC_8BIT ) )
               {
               ptr         += 12;
               blocklength -= 12;
               // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
               voice->bits  = 8;
#endif
               done         = TRUE;
               }
            // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
            else if ( ( BitsPerSample == 16 ) && ( Channels == 1 ) &&
               ( Format == VOC_16BIT ) )
               {
               ptr         += 12;
               blocklength -= 12;
               voice->bits  = 16;
               done         = TRUE;
               }
#endif
            else
               {
               ptr += blocklength;
               }
            break;

         default :
            // Unknown data.  Probably not a VOC file.
            voice->Playing = FALSE;
            done = TRUE;
            break;
         }

      lastblocktype = blocktype;
      }

   if ( voice->Playing )
      {
      voice->NextBlock    = ptr + blocklength;
      voice->sound        = ptr;

      voice->SamplingRate = samplespeed;
      voice->RateScale    = ( voice->SamplingRate * voice->PitchScale ) / MV_MixRate;

      // Multiply by MixBufferSize - 1
      voice->FixedPointBufferSize = ( voice->RateScale * MixBufferSize ) -
         voice->RateScale;

      if ( voice->LoopEnd != NULL )
         {
         if ( blocklength > ( unsigned long )voice->LoopEnd )
            {
            blocklength = ( unsigned long )voice->LoopEnd;
            }
         else
            {
            voice->LoopEnd = ( char * )blocklength;
            }

         voice->LoopStart = voice->sound + ( unsigned long )voice->LoopStart;
         voice->LoopEnd   = voice->sound + ( unsigned long )voice->LoopEnd;
         voice->LoopSize  = voice->LoopEnd - voice->LoopStart;
         }

      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
      if ( voice->bits == 16 )
         {
         blocklength /= 2;
         }
#endif

      voice->position     = 0;
      voice->length       = min( blocklength, 0x8000 );
      voice->BlockLength  = blocklength - voice->length;
      voice->length     <<= 16;

      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
      MV_SetVoiceMixMode( voice );
#endif

      return( KeepPlaying );
      }

   return( NoMoreData );
   }


/*---------------------------------------------------------------------
   Function: MV_GetNextDemandFeedBlock

   Controls playback of demand fed data.
---------------------------------------------------------------------*/

playbackstatus MV_GetNextDemandFeedBlock
   (
   VoiceNode *voice
   )

   {
   if ( voice->BlockLength > 0 )
      {
      voice->position    -= voice->length;
      voice->sound       += voice->length >> 16;
      voice->length       = min( voice->BlockLength, 0x8000 );
      voice->BlockLength -= voice->length;
      voice->length     <<= 16;

      return( KeepPlaying );
      }

   if ( voice->DemandFeed == NULL )
      {
      return( NoMoreData );
      }

   voice->position     = 0;
   ( voice->DemandFeed )( &voice->sound, &voice->BlockLength );
   voice->length       = min( voice->BlockLength, 0x8000 );
   voice->BlockLength -= voice->length;
   voice->length     <<= 16;

   if ( ( voice->length > 0 ) && ( voice->sound != NULL ) )
      {
      return( KeepPlaying );
      }
   return( NoMoreData );
   }


/*---------------------------------------------------------------------
   Function: MV_GetNextRawBlock

   Controls playback of demand fed data.
---------------------------------------------------------------------*/

playbackstatus MV_GetNextRawBlock
   (
   VoiceNode *voice
   )

   {
   if ( voice->BlockLength <= 0 )
      {
      if ( voice->LoopStart == NULL )
         {
         voice->Playing = FALSE;
         return( NoMoreData );
         }

      voice->BlockLength = voice->LoopSize;
      voice->NextBlock   = voice->LoopStart;
      voice->length = 0;
      voice->position = 0;
      }

   voice->sound        = voice->NextBlock;
   voice->position    -= voice->length;
   voice->length       = min( voice->BlockLength, 0x8000 );
   voice->NextBlock   += voice->length;
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   if ( voice->bits == 16 )
      {
      voice->NextBlock += voice->length;
      }
#endif
   voice->BlockLength -= voice->length;
   voice->length     <<= 16;

   return( KeepPlaying );
   }


/*---------------------------------------------------------------------
   Function: MV_GetNextWAVBlock

   Controls playback of demand fed data.
---------------------------------------------------------------------*/

playbackstatus MV_GetNextWAVBlock
   (
   VoiceNode *voice
   )

   {
   if ( voice->BlockLength <= 0 )
      {
      if ( voice->LoopStart == NULL )
         {
         voice->Playing = FALSE;
         return( NoMoreData );
         }

      voice->BlockLength = voice->LoopSize;
      voice->NextBlock   = voice->LoopStart;
      voice->length      = 0;
      voice->position    = 0;
      }

   voice->sound        = voice->NextBlock;
   voice->position    -= voice->length;
   voice->length       = min( voice->BlockLength, 0x8000 );
   voice->NextBlock   += voice->length;
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   if ( voice->bits == 16 )
      {
      voice->NextBlock += voice->length;
      }
#endif
   voice->BlockLength -= voice->length;
   voice->length     <<= 16;

   return( KeepPlaying );
   }


/*---------------------------------------------------------------------
   Function: MV_ServiceRecord

   Starts recording of the waiting buffer.
---------------------------------------------------------------------*/

static void MV_ServiceRecord
   (
   void
   )

   {
   if ( MV_RecordFunc )
      {
      MV_RecordFunc( MV_MixBuffer[ 0 ] + MV_MixPage * MixBufferSize,
         MixBufferSize );
      }

   // Toggle which buffer we'll mix next
   MV_MixPage++;
   if ( MV_MixPage >= NumberOfBuffers )
      {
      MV_MixPage = 0;
      }
   }


/*---------------------------------------------------------------------
   Function: MV_GetVoice

   Locates the voice with the specified handle.
---------------------------------------------------------------------*/

VoiceNode *MV_GetVoice
   (
   int handle
   )

   {
   VoiceNode *voice;
   unsigned  flags;

   flags = DisableInterrupts();

   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960510L)
   voice = VoiceList.start;
   while( voice != NULL )
#else
   for( voice = VoiceList.LIBVER_ASS_MV_VOICESTART; voice != LIBVER_ASS_MV_VOICELISTEND; voice = voice->next )
#endif
      {
      if ( handle == voice->handle )
         {
         break;
         }
      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960510L)
      voice = voice->next;
#endif
      }

   RestoreInterrupts( flags );

   if ( voice == LIBVER_ASS_MV_VOICELISTEND )
      {
      MV_SetErrorCode( MV_VoiceNotFound );
      }

   return( voice );
   }


/*---------------------------------------------------------------------
   Function: MV_VoicePlaying

   Checks if the voice associated with the specified handle is
   playing.
---------------------------------------------------------------------*/

int MV_VoicePlaying
   (
   int handle
   )

   {
   VoiceNode *voice;

   if ( !MV_Installed )
      {
      MV_SetErrorCode( MV_NotInstalled );
      return( FALSE );
      }

   voice = MV_GetVoice( handle );

   if ( voice == NULL )
      {
      return( FALSE );
      }

   return( TRUE );
   }


/*---------------------------------------------------------------------
   Function: MV_KillAllVoices

   Stops output of all currently active voices.
---------------------------------------------------------------------*/

int MV_KillAllVoices
   (
   void
   )

   {
   if ( !MV_Installed )
      {
      MV_SetErrorCode( MV_NotInstalled );
      return( MV_Error );
      }

   // Remove all the voices from the list
   while( VoiceList.LIBVER_ASS_MV_VOICESTART != LIBVER_ASS_MV_VOICELISTEND )
      {
      MV_Kill( VoiceList.LIBVER_ASS_MV_VOICESTART->handle );
      }

   return( MV_Ok );
   }


/*---------------------------------------------------------------------
   Function: MV_Kill

   Stops output of the voice associated with the specified handle.
---------------------------------------------------------------------*/

int MV_Kill
   (
   int handle
   )

   {
   VoiceNode *voice;
   unsigned  flags;
   unsigned  long callbackval;

   if ( !MV_Installed )
      {
      MV_SetErrorCode( MV_NotInstalled );
      return( MV_Error );
      }

   flags = DisableInterrupts();

   voice = MV_GetVoice( handle );
   if ( voice == NULL )
      {
      RestoreInterrupts( flags );
      MV_SetErrorCode( MV_VoiceNotFound );
      return( MV_Error );
      }

   callbackval = voice->callbackval;

   MV_StopVoice( voice );

   RestoreInterrupts( flags );

   if ( MV_CallBackFunc )
      {
      MV_CallBackFunc( callbackval );
      }

   return( MV_Ok );
   }


/*---------------------------------------------------------------------
   Function: MV_VoicesPlaying

   Determines the number of currently active voices.
---------------------------------------------------------------------*/

int MV_VoicesPlaying
   (
   void
   )

   {
   VoiceNode   *voice;
   int         NumVoices = 0;
   unsigned    flags;

   if ( !MV_Installed )
      {
      MV_SetErrorCode( MV_NotInstalled );
      return( 0 );
      }

   flags = DisableInterrupts();

   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960510L)
   voice = VoiceList.start;
   while( voice != NULL )
#else
   for( voice = VoiceList.LIBVER_ASS_MV_VOICESTART; voice != LIBVER_ASS_MV_VOICELISTEND; voice = voice->next )
#endif
      {
      NumVoices++;
      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960510L)
      voice = voice->next;
#endif
      }

   RestoreInterrupts( flags );

   return( NumVoices );
   }


/*---------------------------------------------------------------------
   Function: MV_AllocVoice

   Retrieve an inactive or lower priority voice for output.
---------------------------------------------------------------------*/

VoiceNode *MV_AllocVoice
   (
   int priority
   )

   {
   VoiceNode   *voice;
   VoiceNode   *node;
   unsigned    flags;

//return( NULL );
   if ( MV_Recording )
      {
      return( NULL );
      }

   flags = DisableInterrupts();

   // Check if we have any free voices
   if ( LL_Empty( &VoicePool, next, prev ) )
      {
      // *** VERSIONS RESTORATION ***

      // check if we have a higher priority than a voice that is playing.
#if (LIBVER_ASSREV < 19960510L)
      voice = node = VoiceList.start;
      while( node != NULL )
#else
      voice = VoiceList.LIBVER_ASS_MV_VOICESTART;
      for( node = voice->next; node != LIBVER_ASS_MV_VOICELISTEND; node = node->next )
#endif
         {
         if ( node->priority < voice->priority )
            {
            voice = node;
            }
      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960510L)
         node = node->next;
#endif
         }

      if ( priority >= voice->priority )
         {
         MV_Kill( voice->handle );
         }
      }

   // Check if any voices are in the voice pool
   if ( LL_Empty( &VoicePool, next, prev ) )
      {
      // No free voices
      RestoreInterrupts( flags );
      return( NULL );
      }

   voice = VoicePool.LIBVER_ASS_MV_VOICESTART;
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960510L)
   LL_Remove( VoiceNode, &VoicePool, voice );
#else
   LL_Remove( voice, next, prev );
#endif
   RestoreInterrupts( flags );

   // Find a free voice handle
   do
      {
      MV_VoiceHandle++;
      if ( MV_VoiceHandle < MV_MinVoiceHandle )
         {
         MV_VoiceHandle = MV_MinVoiceHandle;
         }
      }
   while( MV_VoicePlaying( MV_VoiceHandle ) );

   voice->handle = MV_VoiceHandle;

   return( voice );
   }


/*---------------------------------------------------------------------
   Function: MV_VoiceAvailable

   Checks if a voice can be play at the specified priority.
---------------------------------------------------------------------*/

int MV_VoiceAvailable
   (
   int priority
   )

   {
   VoiceNode   *voice;
   VoiceNode   *node;
   unsigned    flags;

   // Check if we have any free voices
   if ( !LL_Empty( &VoicePool, next, prev ) )
      {
      return( TRUE );
      }

   flags = DisableInterrupts();
   // *** VERSIONS RESTORATION ***

   // check if we have a higher priority than a voice that is playing.
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960510L)
   voice = node = VoiceList.start;
   while( node != NULL )
#else
   voice = VoiceList.LIBVER_ASS_MV_VOICESTART;
   for( node = VoiceList.LIBVER_ASS_MV_VOICESTART; node != LIBVER_ASS_MV_VOICELISTEND; node = node->next )
#endif
      {
      if ( node->priority < voice->priority )
         {
         voice = node;
         }
      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960510L)
      node = node->next;
#endif
      }

   RestoreInterrupts( flags );

   if ( ( voice != LIBVER_ASS_MV_VOICELISTEND ) && ( priority >= voice->priority ) )
      {
      return( TRUE );
      }

   return( FALSE );
   }


/*---------------------------------------------------------------------
   Function: MV_SetVoicePitch

   Sets the pitch for the specified voice.
---------------------------------------------------------------------*/

void MV_SetVoicePitch
   (
   VoiceNode *voice,
   unsigned long rate,
   int pitchoffset
   )

   {
   voice->SamplingRate = rate;
   voice->PitchScale   = PITCH_GetScale( pitchoffset );
   voice->RateScale    = ( rate * voice->PitchScale ) / MV_MixRate;

   // Multiply by MixBufferSize - 1
   voice->FixedPointBufferSize = ( voice->RateScale * MixBufferSize ) -
      voice->RateScale;
   }


/*---------------------------------------------------------------------
   Function: MV_SetPitch

   Sets the pitch for the voice associated with the specified handle.
---------------------------------------------------------------------*/

int MV_SetPitch
   (
   int handle,
   int pitchoffset
   )

   {
   VoiceNode *voice;

   if ( !MV_Installed )
      {
      MV_SetErrorCode( MV_NotInstalled );
      return( MV_Error );
      }

   voice = MV_GetVoice( handle );
   if ( voice == NULL )
      {
      MV_SetErrorCode( MV_VoiceNotFound );
      return( MV_Error );
      }

   MV_SetVoicePitch( voice, voice->SamplingRate, pitchoffset );

   return( MV_Ok );
   }


/*---------------------------------------------------------------------
   Function: MV_SetFrequency

   Sets the frequency for the voice associated with the specified handle.
---------------------------------------------------------------------*/

int MV_SetFrequency
   (
   int handle,
   int frequency
   )

   {
   VoiceNode *voice;

   if ( !MV_Installed )
      {
      MV_SetErrorCode( MV_NotInstalled );
      return( MV_Error );
      }

   voice = MV_GetVoice( handle );
   if ( voice == NULL )
      {
      MV_SetErrorCode( MV_VoiceNotFound );
      return( MV_Error );
      }

   MV_SetVoicePitch( voice, frequency, 0 );

   return( MV_Ok );
   }


/*---------------------------------------------------------------------
   Function: MV_GetVolumeTable

   Returns a pointer to the volume table associated with the specified
   volume.
---------------------------------------------------------------------*/

static short *MV_GetVolumeTable
   (
   int vol
   )

   {
   int volume;
   short *table;

   volume = MIX_VOLUME( vol );

   table = &MV_VolumeTable[ volume ];

   return( table );
   }


// *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
/*---------------------------------------------------------------------
   Function: MV_SetVoiceMixMode

   Selects which method should be used to mix the voice.
---------------------------------------------------------------------*/

static void MV_SetVoiceMixMode
   (
   VoiceNode *voice
   )

   {
   unsigned flags;
   int test;

   flags = DisableInterrupts();

   test = T_DEFAULT;
   if ( MV_Bits == 8 )
      {
      test |= T_8BITS;
      }

   if ( voice->bits == 16 )
      {
      test |= T_16BITSOURCE;
      }

   if ( MV_Channels == 1 )
      {
      test |= T_MONO;
      }
   else
      {
      if ( IS_QUIET( voice->RightVolume ) )
         {
         test |= T_RIGHTQUIET;
         }
      else if ( IS_QUIET( voice->LeftVolume ) )
         {
         test |= T_LEFTQUIET;
         }
      }

   // Default case
   voice->mix = MV_Mix8BitMono;

   switch( test )
      {
      case T_8BITS | T_MONO | T_16BITSOURCE :
         voice->mix = MV_Mix8BitMono16;
         break;

      case T_8BITS | T_MONO :
         voice->mix = MV_Mix8BitMono;
         break;

      case T_8BITS | T_16BITSOURCE | T_LEFTQUIET :
         MV_LeftVolume = MV_RightVolume;
         voice->mix = MV_Mix8BitMono16;
         break;

      case T_8BITS | T_LEFTQUIET :
         MV_LeftVolume = MV_RightVolume;
         voice->mix = MV_Mix8BitMono;
         break;

      case T_8BITS | T_16BITSOURCE | T_RIGHTQUIET :
         voice->mix = MV_Mix8BitMono16;
         break;

      case T_8BITS | T_RIGHTQUIET :
         voice->mix = MV_Mix8BitMono;
         break;

      case T_8BITS | T_16BITSOURCE :
         voice->mix = MV_Mix8BitStereo16;
         break;

      case T_8BITS :
         voice->mix = MV_Mix8BitStereo;
         break;

      case T_MONO | T_16BITSOURCE :
         voice->mix = MV_Mix16BitMono16;
         break;

      case T_MONO :
         voice->mix = MV_Mix16BitMono;
         break;

      case T_16BITSOURCE | T_LEFTQUIET :
         MV_LeftVolume = MV_RightVolume;
         voice->mix = MV_Mix16BitMono16;
         break;

      case T_LEFTQUIET :
         MV_LeftVolume = MV_RightVolume;
         voice->mix = MV_Mix16BitMono;
         break;

      case T_16BITSOURCE | T_RIGHTQUIET :
         voice->mix = MV_Mix16BitMono16;
         break;

      case T_RIGHTQUIET :
         voice->mix = MV_Mix16BitMono;
         break;

      case T_16BITSOURCE :
         voice->mix = MV_Mix16BitStereo16;
         break;

      case T_SIXTEENBIT_STEREO :
         voice->mix = MV_Mix16BitStereo;
         break;

      default :
         voice->mix = MV_Mix8BitMono;
      }

   RestoreInterrupts( flags );
   }
#endif // LIBVER_ASSREV >= 19950821L


/*---------------------------------------------------------------------
   Function: MV_SetVoiceVolume

   Sets the stereo and mono volume level of the voice associated
   with the specified handle.
---------------------------------------------------------------------*/

void MV_SetVoiceVolume
   (
   VoiceNode *voice,
   int vol,
   int left,
   int right
   )

   {
   if ( MV_Channels == 1 )
      {
      left  = vol;
      right = vol;
      }

   if ( MV_SwapLeftRight )
      {
      // SBPro uses reversed panning
      voice->LeftVolume  = MV_GetVolumeTable( right );
      voice->RightVolume = MV_GetVolumeTable( left );
      }
   else
      {
      voice->LeftVolume  = MV_GetVolumeTable( left );
      voice->RightVolume = MV_GetVolumeTable( right );
      }

   // *** VERSIONS RESTORATION ***
   // A simpler revision of MV_SetVoiceMixMode in earlier revisions
#if (LIBVER_ASSREV >= 19950821L)
   MV_SetVoiceMixMode( voice );
#else
   if ( MV_Bits == 8 )
      {
      if ( MV_Channels == 1 )
         {
            voice->mix = MV_Mix8BitMonoFast;
         }
      else if ( IS_QUIET( voice->LeftVolume ) )
         {
            MV_LeftVolume = MV_RightVolume;
            voice->mix = MV_Mix8Bit1ChannelFast;
         }
      else if ( IS_QUIET( voice->RightVolume ) )
         {
            voice->mix = MV_Mix8Bit1ChannelFast;
         }
      else
         {
            voice->mix = MV_Mix8BitStereoFast;
         }
      }
   else
      {
      if ( MV_Channels == 1 )
         {
            voice->mix = MV_Mix16BitMonoFast;
         }
      else if ( IS_QUIET( voice->LeftVolume ) )
         {
            MV_LeftVolume = MV_RightVolume;
            voice->mix = MV_Mix16Bit1ChannelFast;
         }
      else if ( IS_QUIET( voice->RightVolume ) )
         {
            voice->mix = MV_Mix16Bit1ChannelFast;
         }
      else
         {
            voice->mix = MV_Mix16BitStereoFast;
         }
      }
#endif
   }


// *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19960510L)
/*---------------------------------------------------------------------
   Function: MV_EndLooping

   Stops the voice associated with the specified handle from looping
   without stoping the sound.
---------------------------------------------------------------------*/

int MV_EndLooping
   (
   int handle
   )

   {
   VoiceNode *voice;
   unsigned flags;

   if ( !MV_Installed )
      {
      MV_SetErrorCode( MV_NotInstalled );
      return( MV_Error );
      }

   flags = DisableInterrupts();

   voice = MV_GetVoice( handle );
   if ( voice == NULL )
      {
      RestoreInterrupts( flags );
      MV_SetErrorCode( MV_VoiceNotFound );
      return( MV_Warning );
      }

   voice->LoopCount = 0;
   voice->LoopStart = NULL;
   voice->LoopEnd   = NULL;

   RestoreInterrupts( flags );

   return( MV_Ok );
   }
#endif // LIBVER_ASSREV >= 19960510L


/*---------------------------------------------------------------------
   Function: MV_SetPan

   Sets the stereo and mono volume level of the voice associated
   with the specified handle.
---------------------------------------------------------------------*/

int MV_SetPan
   (
   int handle,
   int vol,
   int left,
   int right
   )

   {
   VoiceNode *voice;

   if ( !MV_Installed )
      {
      MV_SetErrorCode( MV_NotInstalled );
      return( MV_Error );
      }

   voice = MV_GetVoice( handle );
   if ( voice == NULL )
      {
      MV_SetErrorCode( MV_VoiceNotFound );
      return( MV_Warning );
      }

   MV_SetVoiceVolume( voice, vol, left, right );

   return( MV_Ok );
   }


/*---------------------------------------------------------------------
   Function: MV_Pan3D

   Set the angle and distance from the listener of the voice associated
   with the specified handle.
---------------------------------------------------------------------*/

int MV_Pan3D
   (
   int handle,
   int angle,
   int distance
   )

   {
   int left;
   int right;
   int mid;
   int volume;
   int status;

   if ( distance < 0 )
      {
      distance  = -distance;
      angle    += MV_NumPanPositions / 2;
      }

   volume = MIX_VOLUME( distance );

   // Ensure angle is within 0 - 31
   angle &= MV_MaxPanPosition;

   left  = MV_PanTable[ angle ][ volume ].left;
   right = MV_PanTable[ angle ][ volume ].right;
   mid   = max( 0, 255 - distance );

   status = MV_SetPan( handle, mid, left, right );

   return( status );
   }


/*---------------------------------------------------------------------
   Function: MV_SetReverb

   Sets the level of reverb to add to mix.
---------------------------------------------------------------------*/

void MV_SetReverb
   (
   int reverb
   )

   {
   MV_ReverbLevel = MIX_VOLUME( reverb );
   MV_ReverbTable = &MV_VolumeTable[ MV_ReverbLevel ];
   }


/*---------------------------------------------------------------------
   Function: MV_SetFastReverb

   Sets the level of reverb to add to mix.
---------------------------------------------------------------------*/

void MV_SetFastReverb
   (
   int reverb
   )

   {
   MV_ReverbLevel = max( 0, min( 16, reverb ) );
   MV_ReverbTable = NULL;
   }


// *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
/*---------------------------------------------------------------------
   Function: MV_GetMaxReverbDelay

   Returns the maximum delay time for reverb.
---------------------------------------------------------------------*/

int MV_GetMaxReverbDelay
   (
   void
   )

   {
   int maxdelay;

   maxdelay = MixBufferSize * MV_NumberOfBuffers;

   return maxdelay;
   }


/*---------------------------------------------------------------------
   Function: MV_GetReverbDelay

   Returns the current delay time for reverb.
---------------------------------------------------------------------*/

int MV_GetReverbDelay
   (
   void
   )

   {
   return MV_ReverbDelay / MV_SampleSize;
   }


/*---------------------------------------------------------------------
   Function: MV_SetReverbDelay

   Sets the delay level of reverb to add to mix.
---------------------------------------------------------------------*/

void MV_SetReverbDelay
   (
   int delay
   )

   {
   int maxdelay;

   maxdelay = MV_GetMaxReverbDelay();
   MV_ReverbDelay = max( MixBufferSize, min( delay, maxdelay ) );
   MV_ReverbDelay *= MV_SampleSize;
   }
#endif // LIBVER_ASSREV >= 19950821L


/*---------------------------------------------------------------------
   Function: MV_SetMixMode

   Prepares Multivoc to play stereo of mono digitized sounds.
---------------------------------------------------------------------*/

int MV_SetMixMode
   (
   int numchannels,
   int samplebits
   )

   {
   int mode;

   if ( !MV_Installed )
      {
      MV_SetErrorCode( MV_NotInstalled );
      return( MV_Error );
      }

   mode = 0;
   if ( numchannels == 2 )
      {
      mode |= STEREO;
      }
   if ( samplebits == 16 )
      {
      mode |= SIXTEEN_BIT;
      }

   switch( MV_SoundCard )
      {
      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
      case UltraSound :
         MV_MixMode = mode;
         break;
#endif

      case SoundBlaster :
      case Awe32 :
         MV_MixMode = BLASTER_SetMixMode( mode );
         break;

      case ProAudioSpectrum :
      case SoundMan16 :
         MV_MixMode = PAS_SetMixMode( mode );
         break;

      case SoundScape :
         MV_MixMode = SOUNDSCAPE_SetMixMode( mode );
         break;

      #ifndef SOUNDSOURCE_OFF
      case SoundSource :
      case TandySoundSource :
         MV_MixMode = SS_SetMixMode( mode );
         break;
      #endif
      }

   MV_Channels = 1;
   if ( MV_MixMode & STEREO )
      {
      MV_Channels = 2;
      }

   MV_Bits = 8;
   if ( MV_MixMode & SIXTEEN_BIT )
      {
      MV_Bits = 16;
      }

   MV_BuffShift  = 7 + MV_Channels;
   MV_SampleSize = sizeof( MONO8 ) * MV_Channels;

   if ( MV_Bits == 8 )
      {
      MV_Silence = SILENCE_8BIT;
      }
   else
      {
      MV_Silence     = SILENCE_16BIT;
      MV_BuffShift  += 1;
      MV_SampleSize *= 2;
      }

   MV_BufferSize = MixBufferSize * MV_SampleSize;
   MV_NumberOfBuffers = TotalBufferSize / MV_BufferSize;
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   MV_BufferLength = TotalBufferSize;

   MV_RightChannelOffset = MV_SampleSize / 2;
   if ( ( MV_SoundCard == UltraSound ) && ( MV_Channels == 2 ) )
      {
      MV_SampleSize         /= 2;
      MV_BufferSize         /= 2;
      MV_RightChannelOffset  = MV_BufferSize * MV_NumberOfBuffers;
      MV_BufferLength       /= 2;
      }
#endif

   return( MV_Ok );
   }


/*---------------------------------------------------------------------
   Function: MV_StartPlayback

   Starts the sound playback engine.
---------------------------------------------------------------------*/

int MV_StartPlayback
   (
   void
   )

   {
   int status;
   int buffer;

   // Initialize the buffers
   ClearBuffer_DW( MV_MixBuffer[ 0 ], MV_Silence, TotalBufferSize >> 2 );
   for( buffer = 0; buffer < MV_NumberOfBuffers; buffer++ )
      {
      MV_BufferEmpty[ buffer ] = TRUE;
      }

   // Set the mix buffer variables
   MV_MixPage = 1;

   MV_MixFunction = MV_Mix;

//JIM
//   MV_MixRate = MV_RequestedMixRate;
//   return( MV_Ok );

   // Start playback
   switch( MV_SoundCard )
      {
      case SoundBlaster :
      case Awe32 :
         status = BLASTER_BeginBufferedPlayback( MV_MixBuffer[ 0 ],
            TotalBufferSize, MV_NumberOfBuffers,
            MV_RequestedMixRate, MV_MixMode, MV_ServiceVoc );

         if ( status != BLASTER_Ok )
            {
            MV_SetErrorCode( MV_BlasterError );
            return( MV_Error );
            }

         MV_MixRate = BLASTER_GetPlaybackRate();
         MV_DMAChannel = BLASTER_DMAChannel;
         break;

      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
      case UltraSound :

         status = GUSWAVE_StartDemandFeedPlayback( MV_ServiceGus, 1,
            MV_Bits, MV_RequestedMixRate, 0, ( MV_Channels == 1 ) ?
            0 : 24, 255, 0xffff, 0 );
         if ( status < GUSWAVE_Ok )
            {
            MV_SetErrorCode( MV_BlasterError );
            return( MV_Error );
            }

         if ( MV_Channels == 2 )
            {
            status = GUSWAVE_StartDemandFeedPlayback( MV_ServiceRightGus, 1,
               MV_Bits, MV_RequestedMixRate, 0, 8, 255, 0xffff, 0 );
            if ( status < GUSWAVE_Ok )
               {
               GUSWAVE_KillAllVoices();
               MV_SetErrorCode( MV_BlasterError );
               return( MV_Error );
               }
            }

         MV_MixRate = MV_RequestedMixRate;
         MV_DMAChannel = -1;
         break;
#endif

      case ProAudioSpectrum :
      case SoundMan16 :
         status = PAS_BeginBufferedPlayback( MV_MixBuffer[ 0 ],
            TotalBufferSize, MV_NumberOfBuffers,
            MV_RequestedMixRate, MV_MixMode, MV_ServiceVoc );

         if ( status != PAS_Ok )
            {
            MV_SetErrorCode( MV_PasError );
            return( MV_Error );
            }

         MV_MixRate = PAS_GetPlaybackRate();
         MV_DMAChannel = PAS_DMAChannel;
         break;

      case SoundScape :
         status = SOUNDSCAPE_BeginBufferedPlayback( MV_MixBuffer[ 0 ],
            TotalBufferSize, MV_NumberOfBuffers, MV_RequestedMixRate,
            MV_MixMode, MV_ServiceVoc );

         if ( status != SOUNDSCAPE_Ok )
            {
            MV_SetErrorCode( MV_SoundScapeError );
            return( MV_Error );
            }

         MV_MixRate = SOUNDSCAPE_GetPlaybackRate();
         MV_DMAChannel = SOUNDSCAPE_DMAChannel;
         break;

      #ifndef SOUNDSOURCE_OFF
      case SoundSource :
      case TandySoundSource :
         SS_BeginBufferedPlayback( MV_MixBuffer[ 0 ],
            TotalBufferSize, MV_NumberOfBuffers,
            MV_ServiceVoc );
         MV_MixRate = SS_SampleRate;
         MV_DMAChannel = -1;
         break;
      #endif
      }

   return( MV_Ok );
   }


/*---------------------------------------------------------------------
   Function: MV_StopPlayback

   Stops the sound playback engine.
---------------------------------------------------------------------*/

void MV_StopPlayback
   (
   void
   )

   {
   VoiceNode   *voice;
   VoiceNode   *next;
   unsigned    flags;

   // Stop sound playback
   switch( MV_SoundCard )
      {
      case SoundBlaster :
      case Awe32 :
         BLASTER_StopPlayback();
         break;

      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
      case UltraSound :
         GUSWAVE_KillAllVoices();
         break;
#endif

      case ProAudioSpectrum :
      case SoundMan16 :
         PAS_StopPlayback();
         break;

      case SoundScape :
         SOUNDSCAPE_StopPlayback();
         break;

      #ifndef SOUNDSOURCE_OFF
      case SoundSource :
      case TandySoundSource :
         SS_StopPlayback();
         break;
      #endif
      }

   // Make sure all callbacks are done.
   flags = DisableInterrupts();

   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960510L)
   voice = VoiceList.start;
   while( voice != NULL )
#else
   for( voice = VoiceList.LIBVER_ASS_MV_VOICESTART; voice != LIBVER_ASS_MV_VOICELISTEND; voice = next )
#endif
      {
      next = voice->next;

      MV_StopVoice( voice );

      if ( MV_CallBackFunc )
         {
         MV_CallBackFunc( voice->callbackval );
         }
      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960510L)
      voice = next;
#endif
      }

   RestoreInterrupts( flags );
   }


/*---------------------------------------------------------------------
   Function: MV_StartRecording

   Starts the sound recording engine.
---------------------------------------------------------------------*/

int MV_StartRecording
   (
   int MixRate,
   void ( *function )( char *ptr, int length )
   )

   {
   int status;

   switch( MV_SoundCard )
      {
      case SoundBlaster :
      case Awe32 :
      case ProAudioSpectrum :
      case SoundMan16 :
         break;

      default :
         MV_SetErrorCode( MV_UnsupportedCard );
         return( MV_Error );
         break;
      }

   if ( function == NULL )
      {
      MV_SetErrorCode( MV_NullRecordFunction );
      return( MV_Error );
      }

   MV_StopPlayback();

   // Initialize the buffers
   ClearBuffer_DW( MV_MixBuffer[ 0 ], SILENCE_8BIT, TotalBufferSize >> 2 );

   // Set the mix buffer variables
   MV_MixPage  = 0;

   MV_RecordFunc = function;

   // Start playback
   switch( MV_SoundCard )
      {
      case SoundBlaster :
      case Awe32 :
         status = BLASTER_BeginBufferedRecord( MV_MixBuffer[ 0 ],
            TotalBufferSize, NumberOfBuffers, MixRate, MONO_8BIT,
            MV_ServiceRecord );

         if ( status != BLASTER_Ok )
            {
            MV_SetErrorCode( MV_BlasterError );
            return( MV_Error );
            }
         break;

      case ProAudioSpectrum :
      case SoundMan16 :
         status = PAS_BeginBufferedRecord( MV_MixBuffer[ 0 ],
            TotalBufferSize, NumberOfBuffers, MixRate, MONO_8BIT,
            MV_ServiceRecord );

         if ( status != PAS_Ok )
            {
            MV_SetErrorCode( MV_PasError );
            return( MV_Error );
            }
         break;
      }

   MV_Recording = TRUE;
   return( MV_Ok );
   }


/*---------------------------------------------------------------------
   Function: MV_StopRecord

   Stops the sound record engine.
---------------------------------------------------------------------*/

void MV_StopRecord
   (
   void
   )

   {
   // Stop sound playback
   switch( MV_SoundCard )
      {
      case SoundBlaster :
      case Awe32 :
         BLASTER_StopPlayback();
         break;

      case ProAudioSpectrum :
      case SoundMan16 :
         PAS_StopPlayback();
         break;
      }

   MV_Recording = FALSE;
   MV_StartPlayback();
   }


/*---------------------------------------------------------------------
   Function: MV_StartDemandFeedPlayback

   Plays a digitized sound from a user controlled buffering system.
---------------------------------------------------------------------*/

int MV_StartDemandFeedPlayback
   (
   void ( *function )( char **ptr, unsigned long *length ),
   int rate,
   int pitchoffset,
   int vol,
   int left,
   int right,
   int priority,
   unsigned long callbackval
   )

   {
   VoiceNode *voice;

   if ( !MV_Installed )
      {
      MV_SetErrorCode( MV_NotInstalled );
      return( MV_Error );
      }

   // Request a voice from the voice pool
   voice = MV_AllocVoice( priority );
   if ( voice == NULL )
      {
      MV_SetErrorCode( MV_NoVoices );
      return( MV_Error );
      }

   voice->wavetype    = DemandFeed;
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   voice->bits        = 8;
#endif
   voice->GetSound    = MV_GetNextDemandFeedBlock;
   voice->NextBlock   = NULL;
   voice->DemandFeed  = function;
   voice->LoopStart   = NULL;
   voice->LoopCount   = 0;
   voice->BlockLength = 0;
   voice->position    = 0;
   voice->sound       = NULL;
   voice->length      = 0;
   voice->BlockLength = 0;
   voice->Playing     = TRUE;
   voice->next        = NULL;
   voice->prev        = NULL;
   voice->priority    = priority;
   voice->callbackval = callbackval;

   MV_SetVoicePitch( voice, rate, pitchoffset );
   MV_SetVoiceVolume( voice, vol, left, right );
   MV_PlayVoice( voice );

   return( voice->handle );
   }


/*---------------------------------------------------------------------
   Function: MV_PlayRaw

   Begin playback of sound data with the given sound levels and
   priority.
---------------------------------------------------------------------*/

int MV_PlayRaw
   (
   char *ptr,
   unsigned long length,
   unsigned rate,
   int   pitchoffset,
   int   vol,
   int   left,
   int   right,
   int   priority,
   unsigned long callbackval
   )

   {
   int status;

   status = MV_PlayLoopedRaw( ptr, length, NULL, NULL, rate, pitchoffset,
      vol, left, right, priority, callbackval );

   return( status );
   }


/*---------------------------------------------------------------------
   Function: MV_PlayLoopedRaw

   Begin playback of sound data with the given sound levels and
   priority.
---------------------------------------------------------------------*/

int MV_PlayLoopedRaw
   (
   char *ptr,
   unsigned long  length,
   char *loopstart,
   char *loopend,
   unsigned rate,
   int   pitchoffset,
   int   vol,
   int   left,
   int   right,
   int   priority,
   unsigned long callbackval
   )

   {
   VoiceNode *voice;

   if ( !MV_Installed )
      {
      MV_SetErrorCode( MV_NotInstalled );
      return( MV_Error );
      }

   // Request a voice from the voice pool
   voice = MV_AllocVoice( priority );
   if ( voice == NULL )
      {
      MV_SetErrorCode( MV_NoVoices );
      return( MV_Error );
      }

   voice->wavetype    = Raw;
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   voice->bits        = 8;
#endif
   voice->GetSound    = MV_GetNextRawBlock;
   voice->Playing     = TRUE;
   voice->NextBlock   = ptr;
   voice->position    = 0;
   voice->BlockLength = length;
   voice->length      = 0;
   voice->next        = NULL;
   voice->prev        = NULL;
   voice->priority    = priority;
   voice->callbackval = callbackval;
   voice->LoopStart   = loopstart;
   voice->LoopEnd     = loopend;
   voice->LoopSize    = ( voice->LoopEnd - voice->LoopStart ) + 1;

   MV_SetVoicePitch( voice, rate, pitchoffset );
   MV_SetVoiceVolume( voice, vol, left, right );
   MV_PlayVoice( voice );

   return( voice->handle );
   }


/*---------------------------------------------------------------------
   Function: MV_PlayWAV

   Begin playback of sound data with the given sound levels and
   priority.
---------------------------------------------------------------------*/

int MV_PlayWAV
   (
   char *ptr,
   int   pitchoffset,
   int   vol,
   int   left,
   int   right,
   int   priority,
   unsigned long callbackval
   )

   {
   int status;

   status = MV_PlayLoopedWAV( ptr, -1, -1, pitchoffset, vol, left, right,
      priority, callbackval );

   return( status );
   }


/*---------------------------------------------------------------------
   Function: MV_PlayWAV3D

   Begin playback of sound data at specified angle and distance
   from listener.
---------------------------------------------------------------------*/

int MV_PlayWAV3D
   (
   char *ptr,
   int  pitchoffset,
   int  angle,
   int  distance,
   int  priority,
   unsigned long callbackval
   )

   {
   int left;
   int right;
   int mid;
   int volume;
   int status;

   if ( !MV_Installed )
      {
      MV_SetErrorCode( MV_NotInstalled );
      return( MV_Error );
      }

   if ( distance < 0 )
      {
      distance  = -distance;
      angle    += MV_NumPanPositions / 2;
      }

   volume = MIX_VOLUME( distance );

   // Ensure angle is within 0 - 31
   angle &= MV_MaxPanPosition;

   left  = MV_PanTable[ angle ][ volume ].left;
   right = MV_PanTable[ angle ][ volume ].right;
   mid   = max( 0, 255 - distance );

   status = MV_PlayWAV( ptr, pitchoffset, mid, left, right, priority,
      callbackval );

   return( status );
   }


/*---------------------------------------------------------------------
   Function: MV_PlayLoopedWAV

   Begin playback of sound data with the given sound levels and
   priority.
---------------------------------------------------------------------*/

int MV_PlayLoopedWAV
   (
   char *ptr,
   long  loopstart,
   long  loopend,
   int   pitchoffset,
   int   vol,
   int   left,
   int   right,
   int   priority,
   unsigned long callbackval
   )

   {
   riff_header   *riff;
   format_header *format;
   data_header   *data;
   VoiceNode     *voice;
   int length;
   int absloopend;
   int absloopstart;

   if ( !MV_Installed )
      {
      MV_SetErrorCode( MV_NotInstalled );
      return( MV_Error );
      }

   riff = ( riff_header * )ptr;

   if ( ( strncmp( riff->RIFF, "RIFF", 4 ) != 0 ) ||
      ( strncmp( riff->WAVE, "WAVE", 4 ) != 0 ) ||
      ( strncmp( riff->fmt, "fmt ", 4) != 0 ) )
      {
      MV_SetErrorCode( MV_InvalidWAVFile );
      return( MV_Error );
      }

   format = ( format_header * )( riff + 1 );
   data   = ( data_header * )( ( ( char * )format ) + riff->format_size );

   // Check if it's PCM data.
   if ( format->wFormatTag != 1 )
      {
      MV_SetErrorCode( MV_InvalidWAVFile );
      return( MV_Error );
      }

   if ( format->nChannels != 1 )
      {
      MV_SetErrorCode( MV_InvalidWAVFile );
      return( MV_Error );
      }

   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19950821L)
   if ( format->nBitsPerSample != 8 )
#else
   if ( ( format->nBitsPerSample != 8 ) &&
      ( format->nBitsPerSample != 16 ) )
#endif
      {
      MV_SetErrorCode( MV_InvalidWAVFile );
      return( MV_Error );
      }

   if ( strncmp( data->DATA, "data", 4 ) != 0 )
      {
      MV_SetErrorCode( MV_InvalidWAVFile );
      return( MV_Error );
      }

   // Request a voice from the voice pool
   voice = MV_AllocVoice( priority );
   if ( voice == NULL )
      {
      MV_SetErrorCode( MV_NoVoices );
      return( MV_Error );
      }

   voice->wavetype    = WAV;
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   voice->bits        = format->nBitsPerSample;
#endif
   voice->GetSound    = MV_GetNextWAVBlock;

   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   length = data->size;
   absloopstart = loopstart;
   absloopend   = loopend;
   if ( voice->bits == 16 )
      {
      loopstart  *= 2;
      data->size &= ~1;
      loopend    *= 2;
      length     /= 2;
      }

   loopend    = min( loopend, data->size );
   absloopend = min( absloopend, length );
#endif

   voice->Playing     = TRUE;
   voice->DemandFeed  = NULL;
   voice->LoopStart   = NULL;
   voice->LoopCount   = 0;
   voice->position    = 0;
   voice->length      = 0;
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19950821L)
   voice->BlockLength = min( loopend + 1, data->size );
#else
   voice->BlockLength = absloopend;
#endif
   voice->NextBlock   = ( char * )( data + 1 );
   voice->next        = NULL;
   voice->prev        = NULL;
   voice->priority    = priority;
   voice->callbackval = callbackval;
   voice->LoopStart   = voice->NextBlock + loopstart;
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19950821L)
   voice->LoopEnd     = voice->NextBlock + min( loopend, data->size - 1 );
   voice->LoopSize    = voice->LoopEnd - voice->LoopStart + 1;
#else
   voice->LoopEnd     = voice->NextBlock + loopend;
   voice->LoopSize    = absloopend - absloopstart;
#endif

   if ( ( loopstart >= data->size ) || ( loopstart < 0 ) )
      {
      voice->LoopStart = NULL;
      voice->LoopEnd   = NULL;
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19950821L)
      voice->BlockLength = data->size;
#else
      voice->BlockLength = length;
#endif
      }

   MV_SetVoicePitch( voice, format->nSamplesPerSec, pitchoffset );
   MV_SetVoiceVolume( voice, vol, left, right );
   MV_PlayVoice( voice );

   return( voice->handle );
   }


/*---------------------------------------------------------------------
   Function: MV_PlayVOC3D

   Begin playback of sound data at specified angle and distance
   from listener.
---------------------------------------------------------------------*/

int MV_PlayVOC3D
   (
   char *ptr,
   int  pitchoffset,
   int  angle,
   int  distance,
   int  priority,
   unsigned long callbackval
   )

   {
   int left;
   int right;
   int mid;
   int volume;
   int status;

   if ( !MV_Installed )
      {
      MV_SetErrorCode( MV_NotInstalled );
      return( MV_Error );
      }

   if ( distance < 0 )
      {
      distance  = -distance;
      angle    += MV_NumPanPositions / 2;
      }

   volume = MIX_VOLUME( distance );

   // Ensure angle is within 0 - 31
   angle &= MV_MaxPanPosition;

   left  = MV_PanTable[ angle ][ volume ].left;
   right = MV_PanTable[ angle ][ volume ].right;
   mid   = max( 0, 255 - distance );

   status = MV_PlayVOC( ptr, pitchoffset, mid, left, right, priority,
      callbackval );

   return( status );
   }


/*---------------------------------------------------------------------
   Function: MV_PlayVOC

   Begin playback of sound data with the given sound levels and
   priority.
---------------------------------------------------------------------*/

int MV_PlayVOC
   (
   char *ptr,
   int   pitchoffset,
   int   vol,
   int   left,
   int   right,
   int   priority,
   unsigned long callbackval
   )

   {
   int status;

   status = MV_PlayLoopedVOC( ptr, -1, -1, pitchoffset, vol, left, right,
      priority, callbackval );

   return( status );
   }


/*---------------------------------------------------------------------
   Function: MV_PlayLoopedVOC

   Begin playback of sound data with the given sound levels and
   priority.
---------------------------------------------------------------------*/

int MV_PlayLoopedVOC
   (
   char *ptr,
   long  loopstart,
   long  loopend,
   int   pitchoffset,
   int   vol,
   int   left,
   int   right,
   int   priority,
   unsigned long callbackval
   )

   {
   VoiceNode   *voice;
   int          status;

   if ( !MV_Installed )
      {
      MV_SetErrorCode( MV_NotInstalled );
      return( MV_Error );
      }

   // Make sure it's a valid VOC file.
   status = strncmp( ptr, "Creative Voice File", 19 );
   if ( status != 0 )
      {
      MV_SetErrorCode( MV_InvalidVOCFile );
      return( MV_Error );
      }

   // Request a voice from the voice pool
   voice = MV_AllocVoice( priority );
   if ( voice == NULL )
      {
      MV_SetErrorCode( MV_NoVoices );
      return( MV_Error );
      }

   voice->wavetype    = VOC;
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   voice->bits        = 8;
#endif
   voice->GetSound    = MV_GetNextVOCBlock;
   voice->NextBlock   = ptr + *( unsigned short int * )( ptr + 0x14 );
   voice->DemandFeed  = NULL;
   voice->LoopStart   = NULL;
   voice->LoopCount   = 0;
   voice->BlockLength = 0;
   voice->PitchScale  = PITCH_GetScale( pitchoffset );
   voice->length      = 0;
   voice->next        = NULL;
   voice->prev        = NULL;
   voice->priority    = priority;
   voice->callbackval = callbackval;
   voice->LoopStart   = ( char * )loopstart;
   voice->LoopEnd     = ( char * )loopend;
   voice->LoopSize    = loopend - loopstart + 1;

   if ( loopstart < 0 )
      {
      voice->LoopStart = NULL;
      voice->LoopEnd   = NULL;
      }

   MV_SetVoiceVolume( voice, vol, left, right );
   MV_PlayVoice( voice );

   return( voice->handle );
   }


/*---------------------------------------------------------------------
   Function: MV_LockEnd

   Used for determining the length of the functions to lock in memory.
---------------------------------------------------------------------*/

static void MV_LockEnd
   (
   void
   )

   {
   }


// *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19960510L)
/*---------------------------------------------------------------------
   Function: MV_CreateVolumeTable

   Create the table used to convert sound data to a specific volume
   level.
---------------------------------------------------------------------*/

void MV_CreateVolumeTable
   (
   int index,
   int volume,
   int MaxVolume
   )

   {
   int val;
   int level;
   int i;

   level = ( volume * MaxVolume ) / MV_MaxTotalVolume;
   if ( MV_Bits == 16 )
      {
      for( i = 0; i < 65536; i += 256 )
         {
         val   = i - 0x8000;
         val  *= level;
         val  /= MV_MaxVolume;
         MV_VolumeTable[ index ][ i / 256 ] = val;
         }
      }
   else
      {
      for( i = 0; i < 256; i++ )
         {
         val   = i - 0x80;
         val  *= level;
         val  /= MV_MaxVolume;
         MV_VolumeTable[ volume ][ i ] = val;
         }
      }
   }
#endif // LIBVER_ASSREV >= 19950821L


/*---------------------------------------------------------------------
   Function: MV_CalcVolume

   Create the table used to convert sound data to a specific volume
   level.
---------------------------------------------------------------------*/

void MV_CalcVolume
   (
   int MaxVolume
   )

   {
   int volume;

   // *** VERSIONS RESTORATION ***
   // Partially based on code from MV1.C
#if (LIBVER_ASSREV < 19950821L)
   int bits;
   int val;
   int level;
   int rate;
   int i;
   unsigned    flags;

   flags = DisableInterrupts();


   // For each volume level, create a translation table with the
   // appropriate volume calculated.
   rate  = ( MaxVolume << 16 ) / MV_MaxVolume;
   level = 0;

   bits = 32 - MV_Bits;
#elif (LIBVER_ASSREV < 19960510L)
   int val;
   int level;
   int i;
#endif

   for( volume = 0; volume < 128; volume++ )
      {
      MV_HarshClipTable[ volume ] = 0;
      MV_HarshClipTable[ volume + 384 ] = 255;
      }
   for( volume = 0; volume < 256; volume++ )
      {
      MV_HarshClipTable[ volume + 128 ] = volume;
      }

   // *** VERSIONS RESTORATION ***
   // Partially based on code from MV1.C.
#if (LIBVER_ASSREV < 19950821L)
      for( volume = 0; volume <= MV_MaxVolume; volume++ )
         {
         for( i = 0; i < 256; i++ )
            {
            val = i - 128;
            val *= level;
            val = RoundFixed( val, bits );
            MV_VolumeTable[ volume ][ i ] = val;
            }
         level += rate;
         }

   RestoreInterrupts( flags );
#else
   // For each volume level, create a translation table with the
   // appropriate volume calculated.
   for( volume = 0; volume <= MV_MaxVolume; volume++ )
      {
#if (LIBVER_ASSREV < 19960510L)
      // More-or-less MV_CreateVolumeTable, inlined
      level = ( volume * MaxVolume ) / MV_MaxTotalVolume;
      if ( MV_Bits == 16 )
         {
         for( i = 0; i < 65536; i += 256 )
            {
            val   = i - 0x8000;
            val  *= level;
            val  /= MV_MaxVolume;
            MV_VolumeTable[ volume/*index*/ ][ i / 256 ] = val;
            }
         }
      else
         {
         for( i = 0; i < 256; i++ )
            {
            val   = i - 0x80;
            val  *= level;
            val  /= MV_MaxVolume;
            MV_VolumeTable[ volume ][ i ] = val;
            }
         }
#else
      MV_CreateVolumeTable( volume, volume, MaxVolume );
#endif // LIBVER_ASSREV < 19960510L
      }
#endif // LIBVER_ASSREV < 19950821L
   }


/*---------------------------------------------------------------------
   Function: MV_CalcPanTable

   Create the table used to determine the stereo volume level of
   a sound located at a specific angle and distance from the listener.
---------------------------------------------------------------------*/

void MV_CalcPanTable
   (
   void
   )

   {
   int   level;
   int   angle;
   int   distance;
   int   HalfAngle;
   int   ramp;

   HalfAngle = ( MV_NumPanPositions / 2 );

   for( distance = 0; distance <= MV_MaxVolume; distance++ )
      {
      level = ( 255 * ( MV_MaxVolume - distance ) ) / MV_MaxVolume;
      for( angle = 0; angle <= HalfAngle / 2; angle++ )
         {
         ramp = level - ( ( level * angle ) /
            ( MV_NumPanPositions / 4 ) );

         MV_PanTable[ angle ][ distance ].left = ramp;
         MV_PanTable[ HalfAngle - angle ][ distance ].left = ramp;
         MV_PanTable[ HalfAngle + angle ][ distance ].left = level;
         MV_PanTable[ MV_MaxPanPosition - angle ][ distance ].left = level;

         MV_PanTable[ angle ][ distance ].right = level;
         MV_PanTable[ HalfAngle - angle ][ distance ].right = level;
         MV_PanTable[ HalfAngle + angle ][ distance ].right = ramp;
         MV_PanTable[ MV_MaxPanPosition - angle ][ distance ].right = ramp;
         }
      }
   }


/*---------------------------------------------------------------------
   Function: MV_SetVolume

   Sets the volume of digitized sound playback.
---------------------------------------------------------------------*/

void MV_SetVolume
   (
   int volume
   )

   {
   // *** VERSIONS RESTORATION ***
   // From MV1.C
#if (LIBVER_ASSREV < 19950821L)
   int maxlevel;

#endif
   volume = max( 0, volume );
   volume = min( volume, MV_MaxTotalVolume );

   MV_TotalVolume = volume;

   // *** VERSIONS RESTORATION ***
   // From MV1.C, more-or-less
	
   // Calculate volume table
#if (LIBVER_ASSREV < 19950821L)
   maxlevel = ( 256 * volume ) / MV_MaxTotalVolume;
   //printf( "maxlevel = %d\n", maxlevel );

   // Calculate volume table
   MV_CalcVolume( maxlevel );
#else
   MV_CalcVolume( volume );
#endif
   }


/*---------------------------------------------------------------------
   Function: MV_GetVolume

   Returns the volume of digitized sound playback.
---------------------------------------------------------------------*/

int MV_GetVolume
   (
   void
   )

   {
   return( MV_TotalVolume );
   }


/*---------------------------------------------------------------------
   Function: MV_SetCallBack

   Set the function to call when a voice stops.
---------------------------------------------------------------------*/

void MV_SetCallBack
   (
   void ( *function )( unsigned long )
   )

   {
   MV_CallBackFunc = function;
   }


/*---------------------------------------------------------------------
   Function: MV_SetReverseStereo

   Set the orientation of the left and right channels.
---------------------------------------------------------------------*/

void MV_SetReverseStereo
   (
   int setting
   )

   {
   MV_SwapLeftRight = setting;
   }


/*---------------------------------------------------------------------
   Function: MV_GetReverseStereo

   Returns the orientation of the left and right channels.
---------------------------------------------------------------------*/

int MV_GetReverseStereo
   (
   void
   )

   {
   return( MV_SwapLeftRight );
   }


/*---------------------------------------------------------------------
   Function: MV_TestPlayback

   Checks if playback has started.
---------------------------------------------------------------------*/

int MV_TestPlayback
   (
   void
   )

   {
   unsigned flags;
   long time;
   int  start;
   int  status;
   int  pos;

   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   if ( MV_SoundCard == UltraSound )
      {
      return( MV_Ok );
      }
#endif

   flags = DisableInterrupts();
   _enable();

   status = MV_Error;
   start  = MV_MixPage;
   time   = clock() + CLOCKS_PER_SEC * 2;

   while( clock() < time )
      {
      if ( MV_MixPage != start )
         {
         status = MV_Ok;
         }
      }

   RestoreInterrupts( flags );

   if ( status != MV_Ok )
      {
      // Just in case an error doesn't get reported
      MV_SetErrorCode( MV_DMAFailure );

      switch( MV_SoundCard )
         {
         case SoundBlaster :
         case Awe32 :
            pos = BLASTER_GetCurrentPos();
            break;

         case ProAudioSpectrum :
         case SoundMan16 :
            pos = PAS_GetCurrentPos();
            break;

         case SoundScape :
            pos = SOUNDSCAPE_GetCurrentPos();
            break;

         #ifndef SOUNDSOURCE_OFF
         case SoundSource :
         case TandySoundSource :
            MV_SetErrorCode( MV_SoundSourceFailure );
            pos = -1;
            break;
         #endif

         default :
            MV_SetErrorCode( MV_UnsupportedCard );
            pos = -2;
            break;
         }

      if ( pos > 0 )
         {
         MV_SetErrorCode( MV_IrqFailure );
         }
      else if ( pos == 0 )
         {
         if ( MV_Bits == 16 )
            {
            MV_SetErrorCode( MV_DMA16Failure );
            }
         else
            {
            MV_SetErrorCode( MV_DMAFailure );
            }
         }
      }

   return( status );
   }


/*---------------------------------------------------------------------
   Function: MV_Init

   Perform the initialization of variables and memory used by
   Multivoc.
---------------------------------------------------------------------*/

int MV_Init
   (
   int soundcard,
   int MixRate,
   int Voices,
   int numchannels,
   int samplebits
   )

   {
   char *ptr;
   int  status;
   int  buffer;
   int  index;

   if ( MV_Installed )
      {
      MV_Shutdown();
      }

   MV_SetErrorCode( MV_Ok );

   status = MV_LockMemory();
   if ( status != MV_Ok )
      {
      return( status );
      }

   MV_TotalMemory = Voices * sizeof( VoiceNode ) + sizeof( HARSH_CLIP_TABLE_8 );
   status = USRHOOKS_GetMem( ( void ** )&ptr, MV_TotalMemory );
   if ( status != USRHOOKS_Ok )
      {
      MV_UnlockMemory();
      MV_SetErrorCode( MV_NoMem );
      return( MV_Error );
      }

   status = DPMI_LockMemory( ptr, MV_TotalMemory );
   if ( status != DPMI_Ok )
      {
      USRHOOKS_FreeMem( ptr );
      MV_UnlockMemory();
      MV_SetErrorCode( MV_DPMI_Error );
      return( MV_Error );
      }

   MV_Voices = ( VoiceNode * )ptr;
   MV_HarshClipTable = ptr + ( MV_TotalMemory - sizeof( HARSH_CLIP_TABLE_8 ) );

   // Set number of voices before calculating volume table
   MV_MaxVoices = Voices;

   LL_Reset( &VoiceList, next, prev );
   LL_Reset( &VoicePool, next, prev );

   for( index = 0; index < Voices; index++ )
      {
      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19960510L)
      LL_AddToTail( VoiceNode, &VoicePool, &MV_Voices[ index ] );
#else
      LL_Add( &VoicePool, &MV_Voices[ index ], next, prev );
#endif
      }

   // Allocate mix buffer within 1st megabyte
   status = DPMI_GetDOSMemory( ( void ** )&ptr, &MV_BufferDescriptor,
      2 * TotalBufferSize );

   if ( status )
      {
      DPMI_UnlockMemory( MV_Voices, MV_TotalMemory );
      USRHOOKS_FreeMem( MV_Voices );
      MV_Voices      = NULL;
      MV_TotalMemory = 0;
      MV_UnlockMemory();

      MV_SetErrorCode( MV_NoMem );
      return( MV_Error );
      }

   MV_SetReverseStereo( FALSE );

   // Initialize the sound card
   switch( soundcard )
      {
      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
      case UltraSound :
         status = GUSWAVE_Init( 2 );
         if ( status != GUSWAVE_Ok )
            {
            //JIM
            MV_SetErrorCode( MV_BlasterError );
            }
         break;
#endif

      case SoundBlaster :
      case Awe32 :
         status = BLASTER_Init();
         if ( status != BLASTER_Ok )
            {
            MV_SetErrorCode( MV_BlasterError );
            }

         if ( ( BLASTER_Config.Type == SBPro ) ||
            ( BLASTER_Config.Type == SBPro2 ) )
            {
            MV_SetReverseStereo( TRUE );
            }
         break;

      case ProAudioSpectrum :
      case SoundMan16 :
         status = PAS_Init();
         if ( status != PAS_Ok )
            {
            MV_SetErrorCode( MV_PasError );
            }
         break;

      case SoundScape :
         status = SOUNDSCAPE_Init();
         if ( status != SOUNDSCAPE_Ok )
            {
            MV_SetErrorCode( MV_SoundScapeError );
            }
         break;

      #ifndef SOUNDSOURCE_OFF
      case SoundSource :
      case TandySoundSource :
         status = SS_Init( soundcard );
         if ( status != SS_Ok )
            {
            MV_SetErrorCode( MV_SoundSourceError );
            }
         break;
      #endif

      default :
         MV_SetErrorCode( MV_UnsupportedCard );
         break;
      }

   if ( MV_ErrorCode != MV_Ok )
      {
      status = MV_ErrorCode;

      DPMI_UnlockMemory( MV_Voices, MV_TotalMemory );
      USRHOOKS_FreeMem( MV_Voices );
      MV_Voices      = NULL;
      MV_TotalMemory = 0;

      DPMI_FreeDOSMemory( MV_BufferDescriptor );
      MV_UnlockMemory();

      MV_SetErrorCode( status );
      return( MV_Error );
      }

   MV_SoundCard    = soundcard;
   MV_Installed    = TRUE;
   MV_CallBackFunc = NULL;
   MV_RecordFunc   = NULL;
   MV_Recording    = FALSE;
   MV_ReverbLevel  = 0;
   MV_ReverbTable  = NULL;

   // Set the sampling rate
   MV_RequestedMixRate = MixRate;

   // Set Mixer to play stereo digitized sound
   MV_SetMixMode( numchannels, samplebits );
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   MV_ReverbDelay = MV_BufferSize * 3;
#endif

   // Make sure we don't cross a physical page
   if ( ( ( unsigned long )ptr & 0xffff ) + TotalBufferSize > 0x10000 )
      {
      ptr = ( char * )( ( ( unsigned long )ptr & 0xff0000 ) + 0x10000 );
      }

   MV_MixBuffer[ MV_NumberOfBuffers ] = ptr;
   for( buffer = 0; buffer < MV_NumberOfBuffers; buffer++ )
      {
      MV_MixBuffer[ buffer ] = ptr;
      ptr += MV_BufferSize;
      }

   // Calculate pan table
   MV_CalcPanTable();

   MV_SetVolume( MV_MaxTotalVolume );

   // Start the playback engine
   status = MV_StartPlayback();
   if ( status != MV_Ok )
      {
      // Preserve error code while we shutdown.
      status = MV_ErrorCode;
      MV_Shutdown();
      MV_SetErrorCode( status );
      return( MV_Error );
      }

   if ( MV_TestPlayback() != MV_Ok )
      {
      status = MV_ErrorCode;
      MV_Shutdown();
      MV_SetErrorCode( status );
      return( MV_Error );
      }

   return( MV_Ok );
   }


/*---------------------------------------------------------------------
   Function: MV_Shutdown

   Restore any resources allocated by Multivoc back to the system.
---------------------------------------------------------------------*/

int MV_Shutdown
   (
   void
   )

   {
   int      buffer;
   unsigned flags;

   if ( !MV_Installed )
      {
      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19950821L)
      MV_SetErrorCode( MV_NotInstalled );
      return( MV_Error );
#else
      return( MV_Ok );
#endif
      }

   flags = DisableInterrupts();

   MV_KillAllVoices();

   MV_Installed = FALSE;

   // Stop the sound recording engine
   if ( MV_Recording )
      {
      MV_StopRecord();
      }

   // Stop the sound playback engine
   MV_StopPlayback();

   // Shutdown the sound card
   switch( MV_SoundCard )
      {
      // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
      case UltraSound :
         GUSWAVE_Shutdown();
         break;
#endif

      case SoundBlaster :
      case Awe32 :
         BLASTER_Shutdown();
         break;

      case ProAudioSpectrum :
      case SoundMan16 :
         PAS_Shutdown();
         break;

      case SoundScape :
         SOUNDSCAPE_Shutdown();
         break;

      #ifndef SOUNDSOURCE_OFF
      case SoundSource :
      case TandySoundSource :
         SS_Shutdown();
         break;
      #endif
      }

   RestoreInterrupts( flags );

   // Free any voices we allocated
   DPMI_UnlockMemory( MV_Voices, MV_TotalMemory );
   USRHOOKS_FreeMem( MV_Voices );
   MV_Voices      = NULL;
   MV_TotalMemory = 0;

   LL_Reset( &VoiceList, next, prev );
   LL_Reset( &VoicePool, next, prev );

   MV_MaxVoices = 1;

   // Release the descriptor from our mix buffer
   DPMI_FreeDOSMemory( MV_BufferDescriptor );
   for( buffer = 0; buffer < NumberOfBuffers; buffer++ )
      {
      MV_MixBuffer[ buffer ] = NULL;
      }

   return( MV_Ok );
   }


/*---------------------------------------------------------------------
   Function: MV_UnlockMemory

   Unlocks all neccessary data.
---------------------------------------------------------------------*/

void MV_UnlockMemory
   (
   void
   )

   {
   PITCH_UnlockMemory();

   DPMI_UnlockMemoryRegion( MV_LockStart, MV_LockEnd );
   DPMI_Unlock( MV_VolumeTable );
   DPMI_Unlock( MV_PanTable );
   DPMI_Unlock( MV_Installed );
   DPMI_Unlock( MV_SoundCard );
   DPMI_Unlock( MV_TotalVolume );
   DPMI_Unlock( MV_MaxVoices );
   DPMI_Unlock( MV_BufferSize );
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   DPMI_Unlock( MV_BufferLength );
#endif
   DPMI_Unlock( MV_SampleSize );
   DPMI_Unlock( MV_NumberOfBuffers );
   DPMI_Unlock( MV_MixMode );
   DPMI_Unlock( MV_Channels );
   DPMI_Unlock( MV_Bits );
   DPMI_Unlock( MV_Silence );
   DPMI_Unlock( MV_SwapLeftRight );
   DPMI_Unlock( MV_RequestedMixRate );
   DPMI_Unlock( MV_MixRate );
   DPMI_Unlock( MV_BufferDescriptor );
   DPMI_Unlock( MV_MixBuffer );
   DPMI_Unlock( MV_BufferEmpty );
   DPMI_Unlock( MV_Voices );
   DPMI_Unlock( VoiceList );
   DPMI_Unlock( VoicePool );
   DPMI_Unlock( MV_MixPage );
   DPMI_Unlock( MV_VoiceHandle );
   DPMI_Unlock( MV_CallBackFunc );
   DPMI_Unlock( MV_RecordFunc );
   DPMI_Unlock( MV_Recording );
   DPMI_Unlock( MV_MixFunction );
   DPMI_Unlock( MV_HarshClipTable );
   DPMI_Unlock( MV_MixDestination );
   DPMI_Unlock( MV_LeftVolume );
   DPMI_Unlock( MV_RightVolume );
   DPMI_Unlock( MV_MixPosition );
   DPMI_Unlock( MV_ErrorCode );
   DPMI_Unlock( MV_DMAChannel );
   DPMI_Unlock( MV_BuffShift );
   DPMI_Unlock( MV_ReverbLevel );
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   DPMI_Unlock( MV_ReverbDelay );
#endif
   DPMI_Unlock( MV_ReverbTable );
   }


/*---------------------------------------------------------------------
   Function: MV_LockMemory

   Locks all neccessary data.
---------------------------------------------------------------------*/

int MV_LockMemory
   (
   void
   )

   {
   int status;
   int pitchstatus;

   status  = DPMI_LockMemoryRegion( MV_LockStart, MV_LockEnd );
   status |= DPMI_Lock( MV_VolumeTable );
   status |= DPMI_Lock( MV_PanTable );
   status |= DPMI_Lock( MV_Installed );
   status |= DPMI_Lock( MV_SoundCard );
   status |= DPMI_Lock( MV_TotalVolume );
   status |= DPMI_Lock( MV_MaxVoices );
   status |= DPMI_Lock( MV_BufferSize );
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   status |= DPMI_Lock( MV_BufferLength );
#endif
   status |= DPMI_Lock( MV_SampleSize );
   status |= DPMI_Lock( MV_NumberOfBuffers );
   status |= DPMI_Lock( MV_MixMode );
   status |= DPMI_Lock( MV_Channels );
   status |= DPMI_Lock( MV_Bits );
   status |= DPMI_Lock( MV_Silence );
   status |= DPMI_Lock( MV_SwapLeftRight );
   status |= DPMI_Lock( MV_RequestedMixRate );
   status |= DPMI_Lock( MV_MixRate );
   status |= DPMI_Lock( MV_BufferDescriptor );
   status |= DPMI_Lock( MV_MixBuffer );
   status |= DPMI_Lock( MV_BufferEmpty );
   status |= DPMI_Lock( MV_Voices );
   status |= DPMI_Lock( VoiceList );
   status |= DPMI_Lock( VoicePool );
   status |= DPMI_Lock( MV_MixPage );
   status |= DPMI_Lock( MV_VoiceHandle );
   status |= DPMI_Lock( MV_CallBackFunc );
   status |= DPMI_Lock( MV_RecordFunc );
   status |= DPMI_Lock( MV_Recording );
   status |= DPMI_Lock( MV_MixFunction );
   status |= DPMI_Lock( MV_HarshClipTable );
   status |= DPMI_Lock( MV_MixDestination );
   status |= DPMI_Lock( MV_LeftVolume );
   status |= DPMI_Lock( MV_RightVolume );
   status |= DPMI_Lock( MV_MixPosition );
   status |= DPMI_Lock( MV_ErrorCode );
   status |= DPMI_Lock( MV_DMAChannel );
   status |= DPMI_Lock( MV_BuffShift );
   status |= DPMI_Lock( MV_ReverbLevel );
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   status |= DPMI_Lock( MV_ReverbDelay );
#endif
   status |= DPMI_Lock( MV_ReverbTable );

   pitchstatus = PITCH_LockMemory();
   if ( ( pitchstatus != PITCH_Ok ) || ( status != DPMI_Ok ) )
      {
      MV_UnlockMemory();
      MV_SetErrorCode( MV_DPMI_Error );
      return( MV_Error );
      }

   return( MV_Ok );
   }
