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
   file:   GUS.C

   author: James R. Dose
   date:   September 7, 1994

   Gravis Ultrasound initialization routines.

   (c) Copyright 1994 James R. Dose.  All Rights Reserved.
**********************************************************************/

#include <conio.h>
#include <dos.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include "interrup.h"
#include "newgf1.h"
#include "gusmidi.h"
#include "guswave.h"

#define TRUE  ( 1 == 1 )
#define FALSE ( !TRUE )

#define LOADDS _loadds

#define VOC_8BIT            0x0
#define VOC_CT4_ADPCM       0x1
#define VOC_CT3_ADPCM       0x2
#define VOC_CT2_ADPCM       0x3
#define VOC_16BIT           0x4
#define VOC_ALAW            0x6
#define VOC_MULAW           0x7
#define VOC_CREATIVE_ADPCM  0x200

#define MAX_BLOCK_LENGTH 0x8000

// *** VERSIONS RESTORATION ***
// Uncomment seemingly older values for earlier versions
#if (LIBVER_ASSREV >= 19950821L)
#define GF1BSIZE   896L   /* size of buffer per wav on GUS */
#else
#define GF1BSIZE   512L   /* size of buffer per wav on GUS */
#endif

#if (LIBVER_ASSREV < 19950821L)
#define VOICES     8      /* maximum amount of concurrent wav files */
#else
#define VOICES     2      /* maximum amount of concurrent wav files */
#endif
#define MAX_VOICES 32     /* This should always be 32 */
#define MAX_VOLUME 4095
#define BUFFER     2048U  /* size of DMA buffer for patch loading */

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
   KeepPlaying,
   SoundDone
   } playbackstatus;


typedef volatile struct VoiceNode
   {
   struct VoiceNode *next;
   struct VoiceNode *prev;

   wavedata      wavetype;
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV >= 19950821L)
   int           bits;
#endif
   playbackstatus ( *GetSound )( struct VoiceNode *voice );

   int num;

   unsigned long  mem;           /* location in ultrasound memory */
   int            Active;        /* this instance in use */
   int            GF1voice;      /* handle to active voice */

   char          *NextBlock;
   char          *LoopStart;
   char          *LoopEnd;
   unsigned       LoopCount;
   unsigned long  LoopSize;
   unsigned long  BlockLength;

   unsigned long  PitchScale;

   unsigned char *sound;
   unsigned long  length;
   unsigned long  SamplingRate;
   unsigned long  RateScale;
   int            Playing;

   int    handle;
   int    priority;

   void          ( *DemandFeed )( char **ptr, unsigned long *length );

   unsigned long callbackval;

   int    Volume;
   int    Pan;
   }
VoiceNode;

typedef struct
   {
   VoiceNode *start;
   VoiceNode *end;
   }
voicelist;

typedef volatile struct voicestatus
   {
   VoiceNode *Voice;
   int playing;
   }
voicestatus;

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

static playbackstatus GUSWAVE_GetNextVOCBlock( VoiceNode *voice );

static int GUSWAVE_Play( VoiceNode *voice, int angle, int volume, int channels );

static int GUSWAVE_InitVoices( void );


// size of DMA buffer for patch loading
#define DMABUFFSIZE 2048U

struct gf1_dma_buff GUS_HoldBuffer;
static int          HoldBufferAllocated = FALSE;

static int GUS_Installed = 0;

extern VoiceNode   GUSWAVE_Voices[ VOICES ];
extern int GUSWAVE_Installed;

static unsigned long GUS_TotalMemory;
int           GUS_MemConfig;

int GUS_AuxError  = 0;

int GUS_ErrorCode = GUS_Ok;

#define GUS_SetErrorCode( status ) \
   GUS_ErrorCode   = ( status );


/*---------------------------------------------------------------------
   Function: D32DosMemAlloc

   Allocate a block of Conventional memory.
---------------------------------------------------------------------*/

static void *D32DosMemAlloc
   (
   unsigned size
   )

   {
   union REGS r;

   // DPMI allocate DOS memory
   r.x.eax = 0x0100;

   // Number of paragraphs requested
   r.x.ebx = ( size + 15 ) >> 4;
   int386( 0x31, &r, &r );
   if ( r.x.cflag )
      {
      // Failed
      return( NULL );
      }

   return( ( void * )( ( r.x.eax & 0xFFFF ) << 4 ) );
   }


/*---------------------------------------------------------------------
   Function: GUS_Init

   Initializes the Gravis Ultrasound for sound and music playback.
---------------------------------------------------------------------*/

int GUS_Init
   (
   void
   )

   {
   struct load_os os;
   int ret;

   if ( GUS_Installed > 0 )
      {
      GUS_Installed++;
      return( GUS_Ok );
      }

   GUS_SetErrorCode( GUS_Ok );

   GUS_Installed = 0;

//FIXME   GetUltraCfg( &os );

   if ( os.forced_gf1_irq > 7 )
      {
      GUS_SetErrorCode( GUS_InvalidIrq );
      return( GUS_Error );
      }

   if ( !HoldBufferAllocated )
      {
      GUS_HoldBuffer.vptr = D32DosMemAlloc( DMABUFFSIZE );
      if ( GUS_HoldBuffer.vptr == NULL )
         {
         GUS_SetErrorCode( GUS_OutOfDosMemory );
         return( GUS_Error );
         }
      GUS_HoldBuffer.paddr = ( unsigned long )GUS_HoldBuffer.vptr;

      HoldBufferAllocated = TRUE;
      }

   os.voices = 24;
   ret = 1; //FIXME gf1_load_os( &os );
   if ( ret )
      {
      GUS_AuxError = ret;
      GUS_SetErrorCode( GUS_GF1Error );
      return( GUS_Error );
      }

   GUS_TotalMemory = 0; //FIXME gf1_mem_avail();
   GUS_MemConfig   = ( GUS_TotalMemory - 1 ) >> 18;

   GUS_Installed = 1;
   return( GUS_Ok );
   }


/*---------------------------------------------------------------------
   Function: GUS_Shutdown

   Ends use of the Gravis Ultrasound.  Must be called the same number
   of times as GUS_Init.
---------------------------------------------------------------------*/

void GUS_Shutdown
   (
   void
   )

   {
   // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19950821L)
   GUS_SetErrorCode( GUS_Ok );
#endif
   if ( GUS_Installed > 0 )
      {
      GUS_Installed--;
      if ( GUS_Installed == 0 )
         {
//FIXME         gf1_unload_os();
         }
      }
   }


/*---------------------------------------------------------------------
   Function: GUSWAVE_Shutdown

   Terminates use of the Gravis Ultrasound for digitized sound playback.
---------------------------------------------------------------------*/

void GUSWAVE_Shutdown
   (
   void
   )

   {
   int i;

   if ( GUSWAVE_Installed )
      {
      GUSWAVE_KillAllVoices();

      // free memory
      for ( i = 0; i < VOICES; i++ )
         {
         if ( GUSWAVE_Voices[ i ].mem != 0 )
            {
//FIXME            gf1_free( GUSWAVE_Voices[ i ].mem );
            GUSWAVE_Voices[ i ].mem = NULL;
            }
         }

      GUS_Shutdown();
      GUSWAVE_Installed = FALSE;
      }
   }
