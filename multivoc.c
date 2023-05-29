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
#include "interrup.h"
#include "dma.h"

#define LL_Empty(a,b,c) ((a)->start == NULL)
#define LL_Reset( list, next, prev ) (list)->start = NULL; (list)->end = NULL

#include "sndcards.h"
#include "blaster.h"
#include "pas16.h"
#include "guswave.h"
#include "pitch.h"
#include "multivoc.h"


#define TRUE  ( 1 == 1 )
#define FALSE ( !TRUE )

#define VOC_8BIT            0x0
#define VOC_CT4_ADPCM       0x1
#define VOC_CT3_ADPCM       0x2
#define VOC_CT2_ADPCM       0x3
#define VOC_16BIT           0x4
#define VOC_ALAW            0x6
#define VOC_MULAW           0x7
#define VOC_CREATIVE_ADPCM  0x200

#define T_SIXTEENBIT_STEREO 0
#define T_8BITS       1
#define T_MONO        2
#define T_16BITSOURCE 4
#define T_LEFTQUIET   8
#define T_RIGHTQUIET  16
#define T_DEFAULT     T_SIXTEENBIT_STEREO

#define MV_MaxPanPosition  31
#define MV_NumPanPositions ( MV_MaxPanPosition + 1 )
#define MV_MaxTotalVolume  255
// *** VERSIONS RESTORATION ***
// Uncomment for earlier versions
#if (LIBVER_ASSREV < 19960510L)
#define MV_MaxVolume       63
#endif
#define MV_NumVoices       8

// *** VERSIONS RESTORATION ***
// Uncomment line for earlier revisions
#if (LIBVER_ASSREV >= 19960510L)
#define MIX_VOLUME( volume ) \
   ( ( max( 0, min( ( volume ), 255 ) ) * ( MV_MaxVolume + 1 ) ) >> 8 )
#else
#define MIX_VOLUME( volume ) \
   ( ( max( 0, min( ( volume ), 255 ) ) ) >> 2 )
#endif

//#define SILENCE_16BIT     0x80008000
#define SILENCE_16BIT     0
#define SILENCE_8BIT      0x80808080
//#define SILENCE_16BIT_PAS 0

#define MixBufferSize     256

#define NumberOfBuffers   16
#define TotalBufferSize   ( MixBufferSize * NumberOfBuffers )

#define PI                3.1415926536

typedef enum
   {
   Raw,
   VOC,
   DemandFeed,
   WAV
   } wavedata;

typedef enum
   {
   NoMoreData,
   KeepPlaying
   } playbackstatus;


typedef struct VoiceNode
   {
   struct VoiceNode *next;
   struct VoiceNode *prev;

   wavedata      wavetype;
   char          bits;

   playbackstatus ( *GetSound )( struct VoiceNode *voice );

   void ( *mix )( unsigned long position, unsigned long rate,
      char *start, unsigned long length );

   char         *NextBlock;
   char         *LoopStart;
   char         *LoopEnd;
   unsigned      LoopCount;
   unsigned long LoopSize;
   unsigned long BlockLength;

   unsigned long PitchScale;
   unsigned long FixedPointBufferSize;

   char         *sound;
   unsigned long length;
   unsigned long SamplingRate;
   unsigned long RateScale;
   unsigned long position;
   int           Playing;

   int           handle;
   int           priority;

   void          ( *DemandFeed )( char **ptr, unsigned long *length );

   short        *LeftVolume;
   short        *RightVolume;

   unsigned long callbackval;

   } VoiceNode;

typedef struct
   {
   VoiceNode *start;
   VoiceNode *end;
   } VList;

typedef struct
   {
   char left;
   char right;
   } Pan;

typedef signed short MONO16;
typedef signed char  MONO8;

typedef struct
   {
   MONO16 left;
   MONO16 right;
//   unsigned short left;
//   unsigned short right;
   } STEREO16;

typedef struct
   {
   MONO16 left;
   MONO16 right;
   } SIGNEDSTEREO16;

typedef struct
   {
//   MONO8 left;
//   MONO8 right;
   char left;
   char right;
   } STEREO8;

typedef struct
   {
   char          RIFF[ 4 ];
   unsigned long file_size;
   char          WAVE[ 4 ];
   char          fmt[ 4 ];
   unsigned long format_size;
   } riff_header;

typedef struct
   {
   unsigned short wFormatTag;
   unsigned short nChannels;
   unsigned long  nSamplesPerSec;
   unsigned long  nAvgBytesPerSec;
   unsigned short nBlockAlign;
   unsigned short nBitsPerSample;
   } format_header;

typedef struct
   {
   unsigned char DATA[ 4 ];
   unsigned long size;
   } data_header;

typedef MONO8  VOLUME8[ 256 ];
typedef MONO16 VOLUME16[ 256 ];

typedef char HARSH_CLIP_TABLE_8[ MV_NumVoices * 256 ];

static void MV_Mix( VoiceNode *voice, int buffer );
static void MV_PlayVoice( VoiceNode *voice );
static void MV_StopVoice( VoiceNode *voice );
static void MV_ServiceVoc( void );

static playbackstatus MV_GetNextRawBlock( VoiceNode *voice );

static VoiceNode *MV_GetVoice( int handle );
static VoiceNode *MV_AllocVoice( int priority );

static short     *MV_GetVolumeTable( int vol );

static void       MV_SetVoiceMixMode( VoiceNode *voice );

static void       MV_SetVoicePitch( VoiceNode *voice, unsigned long rate, int pitchoffset );
static void       MV_CalcVolume( int MaxLevel );
static void       MV_CalcPanTable( void );

#define ATR_INDEX               0x3c0
#define STATUS_REGISTER_1       0x3da

#define SetBorderColor(color) \
   { \
   inp  (STATUS_REGISTER_1); \
   outp (ATR_INDEX,0x31);    \
   outp (ATR_INDEX,color);   \
   }

void ClearBuffer_DW( void *ptr, unsigned data, int length );

#pragma aux ClearBuffer_DW = \
   "cld",                    \
   "push   es",              \
   "push   ds",              \
   "pop    es",              \
   "rep    stosd",           \
   "pop    es",              \
parm [ edi ] [ eax ] [ ecx ] modify exact [ ecx edi ];

// *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19950821L)
void MV_Mix8BitMonoFast( unsigned long position, unsigned long rate,
   char *start, unsigned long length );

void MV_Mix8BitStereoFast( unsigned long position, unsigned long rate,
   char *start, unsigned long length );

void MV_Mix8Bit1ChannelFast( unsigned long position, unsigned long rate,
   char *start, unsigned long length );

void MV_Mix16BitMonoFast( unsigned long position, unsigned long rate,
   char *start, unsigned long length );

void MV_Mix16BitStereoFast( unsigned long position, unsigned long rate,
   char *start, unsigned long length );

void MV_Mix16Bit1ChannelFast( unsigned long position, unsigned long rate,
   char *start, unsigned long length );
#else // LIBVER_ASSREV >= 19950821L
void MV_Mix8BitMono( unsigned long position, unsigned long rate,
   char *start, unsigned long length );

void MV_Mix8BitStereo( unsigned long position,
   unsigned long rate, char *start, unsigned long length );

void MV_Mix16BitMono( unsigned long position,
   unsigned long rate, char *start, unsigned long length );

void MV_Mix16BitStereo( unsigned long position,
   unsigned long rate, char *start, unsigned long length );

void MV_Mix16BitMono16( unsigned long position,
   unsigned long rate, char *start, unsigned long length );

void MV_Mix8BitMono16( unsigned long position, unsigned long rate,
   char *start, unsigned long length );

void MV_Mix8BitStereo16( unsigned long position,
   unsigned long rate, char *start, unsigned long length );

void MV_Mix16BitStereo16( unsigned long position,
   unsigned long rate, char *start, unsigned long length );

void MV_16BitReverb( char *src, char *dest, VOLUME16 *volume, int count );
#pragma aux MV_16BitReverb parm [eax] [edx] [ebx] [ecx] modify exact [eax ebx ecx edx esi edi]
void MV_8BitReverb( signed char *src, signed char *dest, VOLUME16 *volume, int count );
#pragma aux MV_8BitReverb parm [eax] [edx] [ebx] [ecx] modify exact [eax ebx ecx edx esi edi]
void MV_16BitReverbFast( char *src, char *dest, int count, int shift );
#pragma aux MV_16BitReverbFast parm [eax] [edx] [ebx] [ecx] modify exact [eax ebx ecx edx esi edi]
void MV_8BitReverbFast( signed char *src, signed char *dest, int count, int shift );
#pragma aux MV_8BitReverbFast parm [eax] [edx] [ebx] [ecx] modify exact [eax ebx ecx edx esi edi]
#endif // LIBVER_ASSREV < 19950821L


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
static char *MV_MixBuffer[ NumberOfBuffers + 1 ];

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

static int MV_ErrorCode = MV_Ok;

#define MV_SetErrorCode( status ) \
   MV_ErrorCode   = ( status );


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
   Linked list management routines.
---------------------------------------------------------------------*/

enum LL_Errors
   {
   LL_Warning = -2,
   LL_Error   = -1,
   LL_Ok      = 0
   };

typedef struct list
   {
   void *start;
   void *end;
   } list;

#define OFFSET( structure, offset ) \
   ( *( ( char ** )&( structure )[ offset ] ) )

#define LL_AddToHead( type, listhead, node )         \
    LL_AddNode( ( char * )( node ),                  \
                ( char ** )&( ( listhead )->start ), \
                ( char ** )&( ( listhead )->end ),   \
                ( int )&( ( type * ) 0 )->next,      \
                ( int )&( ( type * ) 0 )->prev )

#define LL_AddToTail( type, listhead, node )         \
    LL_AddNode( ( char * )( node ),                  \
                ( char ** )&( ( listhead )->end ),   \
                ( char ** )&( ( listhead )->start ), \
                ( int )&( ( type * ) 0 )->prev,      \
                ( int )&( ( type * ) 0 )->next )

#define LL_Remove( type, listhead, node )               \
    LL_RemoveNode( ( char * )( node ),                  \
                   ( char ** )&( ( listhead )->start ), \
                   ( char ** )&( ( listhead )->end ),   \
                   ( int )&( ( type * ) 0 )->next,      \
                   ( int )&( ( type * ) 0 )->prev )

#define LL_NextNode( node )     ( ( node )->next )
#define LL_PreviousNode( node ) ( ( node )->prev )

static void LL_AddNode
   (
   char *item,
   char **head,
   char **tail,
   int next,
   int prev
   )

   {
   OFFSET( item, prev ) = NULL;
   OFFSET( item, next ) = *head;

   if ( *head )
      {
      OFFSET( *head, prev ) = item;
      }
   else
      {
      *tail = item;
      }

   *head = item;
   }

static void LL_RemoveNode
   (
   char *item,
   char **head,
   char **tail,
   int next,
   int prev
   )

   {
   if ( OFFSET( item, prev ) == NULL )
      {
      *head = OFFSET( item, next );
      }
   else
      {
      OFFSET( OFFSET( item, prev ), next ) = OFFSET( item, next );
      }

   if ( OFFSET( item, next ) == NULL )
      {
      *tail = OFFSET( item, prev );
      }
   else
      {
      OFFSET( OFFSET( item, next ), prev ) = OFFSET( item, prev );
      }

   OFFSET( item, next ) = NULL;
   OFFSET( item, prev ) = NULL;
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


static int leftpage  = -1;
static int rightpage = -1;

static void MV_ServiceGus( char **ptr, unsigned long *length )
   {
   if ( leftpage == MV_MixPage )
      {
      MV_ServiceVoc();
      }

   leftpage = MV_MixPage;

   *ptr = MV_MixBuffer[ MV_MixPage ];
   *length = MV_BufferSize;
   }

static void MV_ServiceRightGus( char **ptr, unsigned long *length )
   {
   if ( rightpage == MV_MixPage )
      {
      MV_ServiceVoc();
      }

   rightpage = MV_MixPage;

   *ptr = MV_MixBuffer[ MV_MixPage ] + MV_RightChannelOffset;
   *length = MV_BufferSize;
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

static int MV_KillAllVoices
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

static void MV_SetVoiceVolume
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
   Function: MV_SetMixMode

   Prepares Multivoc to play stereo of mono digitized sounds.
---------------------------------------------------------------------*/

static int MV_SetMixMode
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

static int MV_StartPlayback
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
      }

   return( MV_Ok );
   }


/*---------------------------------------------------------------------
   Function: MV_StopPlayback

   Stops the sound playback engine.
---------------------------------------------------------------------*/

static void MV_StopPlayback
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
   Function: MV_StopRecord

   Stops the sound record engine.
---------------------------------------------------------------------*/

static void MV_StopRecord
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

static int MV_PlayLoopedRaw
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
   Function: MV_SetReverseStereo

   Set the orientation of the left and right channels.
---------------------------------------------------------------------*/

static void MV_SetReverseStereo
   (
   int setting
   )

   {
   MV_SwapLeftRight = setting;
   }


/*---------------------------------------------------------------------
   Function: MV_TestPlayback

   Checks if playback has started.
---------------------------------------------------------------------*/

static int MV_TestPlayback
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
   Error definitions
---------------------------------------------------------------------*/

enum USRHOOKS_Errors
   {
   USRHOOKS_Warning = -2,
   USRHOOKS_Error   = -1,
   USRHOOKS_Ok      = 0
   };


/*---------------------------------------------------------------------
   Function: USRHOOKS_GetMem

   Allocates the requested amount of memory and returns a pointer to
   its location, or NULL if an error occurs.  NOTE: pointer is assumed
   to be dword aligned.
---------------------------------------------------------------------*/

static int USRHOOKS_GetMem
   (
   void **ptr,
   unsigned long size
   )

   {
   void *memory;

   memory = malloc( size );
   if ( memory == NULL )
      {
      return( USRHOOKS_Error );
      }

   *ptr = memory;

   return( USRHOOKS_Ok );
   }


/*---------------------------------------------------------------------
   Function: USRHOOKS_FreeMem

   Deallocates the memory associated with the specified pointer.
---------------------------------------------------------------------*/

static int USRHOOKS_FreeMem
   (
   void *ptr
   )

   {
   if ( ptr == NULL )
      {
      return( USRHOOKS_Error );
      }

   free( ptr );

   return( USRHOOKS_Ok );
   }


/*---------------------------------------------------------------------
   Function: MV_Init

   Perform the initialization of variables and memory used by
   Multivoc.
---------------------------------------------------------------------*/

static int  DPMI_GetDOSMemory( void **ptr, int *descriptor, unsigned length );
#pragma aux DPMI_GetDOSMemory = \
   "mov    eax, 0100h",         \
   "add    ebx, 15",            \
   "shr    ebx, 4",             \
   "int    31h",                \
   "jc     DPMI_Exit",          \
   "movzx  eax, ax",            \
   "shl    eax, 4",             \
   "mov    [ esi ], eax",       \
   "mov    [ edi ], edx",       \
   "sub    eax, eax",           \
   "DPMI_Exit:",                \
   parm [ esi ] [ edi ] [ ebx ] modify exact [ eax ebx edx ];

static int  DPMI_FreeDOSMemory( int descriptor );
#pragma aux DPMI_FreeDOSMemory = \
   "mov    eax, 0101h",          \
   "int    31h",                 \
   "jc     DPMI_Exit",           \
   "sub    eax, eax",            \
   "DPMI_Exit:",                 \
   parm [ edx ] modify exact [ eax ];

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

   MV_TotalMemory = Voices * sizeof( VoiceNode ) + sizeof( HARSH_CLIP_TABLE_8 );
   status = USRHOOKS_GetMem( ( void ** )&ptr, MV_TotalMemory );
   if ( status != USRHOOKS_Ok )
      {
      MV_SetErrorCode( MV_NoMem );
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
      USRHOOKS_FreeMem( MV_Voices );
      MV_Voices      = NULL;
      MV_TotalMemory = 0;

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

      default :
         MV_SetErrorCode( MV_UnsupportedCard );
         break;
      }

   if ( MV_ErrorCode != MV_Ok )
      {
      status = MV_ErrorCode;

      USRHOOKS_FreeMem( MV_Voices );
      MV_Voices      = NULL;
      MV_TotalMemory = 0;

      DPMI_FreeDOSMemory( MV_BufferDescriptor );

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
   MV_ReverbDelay = MV_BufferSize * 3;

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
      }

   RestoreInterrupts( flags );

   // Free any voices we allocated
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
