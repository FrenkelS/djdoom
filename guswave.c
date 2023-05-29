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
   file:   GUSWAVE.C

   author: James R. Dose
   date:   March 23, 1994

   Digitized sound playback routines for the Gravis Ultrasound.

   (c) Copyright 1994 James R. Dose.  All Rights Reserved.
**********************************************************************/

#include <stdlib.h>
#include <conio.h>
#include <dos.h>
#include <stdio.h>
#include <io.h>
#include <string.h>
#include <stdarg.h>
#include "interrup.h"
#include "ll_man.h"
#include "pitch.h"
#include "multivoc.h"
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


#define ATR_INDEX               0x3c0
#define STATUS_REGISTER_1       0x3da

#define SetBorderColor(color) \
   { \
   inp  (STATUS_REGISTER_1); \
   outp (ATR_INDEX,0x31);    \
   outp (ATR_INDEX,color);   \
   }

static const int GUSWAVE_PanTable[ 32 ] =
   {
      8,  9, 10, 11, 11, 12, 13, 14,
     15, 14, 13, 12, 11, 10,  9,  8,
      7,  6,  5,  4,  4,  3,  2,  1,
      0,  1,  2,  3,  4,  5,  6,  7
   };

static voicelist VoiceList;
static voicelist VoicePool;

static voicestatus VoiceStatus[ MAX_VOICES ];
//static
VoiceNode GUSWAVE_Voices[ VOICES ];

static int GUSWAVE_VoiceHandle  = GUSWAVE_MinVoiceHandle;
static int GUSWAVE_MaxVoices = VOICES;
//static
int GUSWAVE_Installed = FALSE;

static void ( *GUSWAVE_CallBackFunc )( unsigned long ) = NULL;

// current volume for dig audio - from 0 to 4095
static int GUSWAVE_Volume = MAX_VOLUME;

static int GUSWAVE_SwapLeftRight = FALSE;

extern int GUSMIDI_Installed;

static int GUSWAVE_ErrorCode = GUSWAVE_Ok;

#define GUSWAVE_SetErrorCode( status ) \
   GUSWAVE_ErrorCode   = ( status );


/*---------------------------------------------------------------------
   Function: GUSWAVE_CallBack

   GF1 callback service routine.
---------------------------------------------------------------------*/

static unsigned char GUS_Silence8[ 1024 ] =
   {
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
   };

static unsigned short GUS_Silence16[ 512 ] =
   {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
   };

static int LOADDS GUSWAVE_CallBack
   (
   int reason,
   int voice,
   unsigned char **buf,
   unsigned long *size
   )

   {
   VoiceNode *Voice;
   playbackstatus status;

   // this function is called from an interrupt
   // remember not to make any DOS or BIOS calls from here
   // also don't call any C library functions unless you are sure that
   // they are reentrant
   // restore our DS register

   if ( VoiceStatus[ voice ].playing == FALSE )
      {
      return( DIG_DONE );
      }

   if ( reason == DIG_MORE_DATA )
      {
      Voice = VoiceStatus[ voice ].Voice;

      if ( ( Voice != NULL ) && ( Voice->Playing ) )
         {
         status = Voice->GetSound( Voice );
         if ( status != SoundDone )
            {
            if ( ( Voice->sound == NULL ) || ( status == NoMoreData ) )
               {
               if ( Voice->bits == 8 )
                  {
                  *buf = GUS_Silence8;
                  }
               else
                  {
                  *buf = ( unsigned char * )GUS_Silence16;
                  }
               *size = 256;
               }
            else
               {
               *buf  = Voice->sound;
               *size = Voice->length;
               }
            return( DIG_MORE_DATA );
            }
         }
      return( DIG_DONE );
      }

   if ( reason == DIG_DONE )
      {
      Voice = VoiceStatus[ voice ].Voice;
      VoiceStatus[ voice ].playing = FALSE;

      if ( Voice != NULL )
         {
         Voice->Active   = FALSE;
         Voice->Playing  = FALSE;

// I'm commenting this out because a -1 could cause a crash if it
// is sent to the GF1 code.  This shouldn't be necessary since
// Active should be false when GF1voice is -1, but this is just
// a precaution.  Adjust the pan on the wrong voice is a lot
// more pleasant than a crash!
//         Voice->GF1voice = -1;

         LL_Remove( VoiceNode, &VoiceList, Voice );
         LL_AddToTail( VoiceNode, &VoicePool, Voice );
         }

      if ( GUSWAVE_CallBackFunc )
         {
         GUSWAVE_CallBackFunc( Voice->callbackval );
         }
      }

   return( DIG_DONE );
   }


/*---------------------------------------------------------------------
   Function: GUSWAVE_GetVoice

   Locates the voice with the specified handle.
---------------------------------------------------------------------*/

static VoiceNode *GUSWAVE_GetVoice
   (
   int handle
   )

   {
   VoiceNode *voice;
   unsigned  flags;

   flags = DisableInterrupts();

   voice = VoiceList.start;

   while( voice != NULL )
      {
      if ( handle == voice->handle )
         {
         break;
         }

      voice = voice->next;
      }

   RestoreInterrupts( flags );

   if ( voice == NULL )
      {
      GUSWAVE_SetErrorCode( GUSWAVE_VoiceNotFound );
      }

   return( voice );
   }


/*---------------------------------------------------------------------
   Function: GUSWAVE_VoicePlaying

   Checks if the voice associated with the specified handle is
   playing.
---------------------------------------------------------------------*/

static int GUSWAVE_VoicePlaying
   (
   int handle
   )

   {
   VoiceNode   *voice;

   voice = GUSWAVE_GetVoice( handle );
   if ( voice != NULL )
      {
      return( voice->Active );
      }

   return( FALSE );
   }


/*---------------------------------------------------------------------
   Function: GUSWAVE_VoicesPlaying

   Determines the number of currently active voices.
---------------------------------------------------------------------*/

static int GUSWAVE_VoicesPlaying
   (
   void
   )

   {
   int         index;
   int         NumVoices = 0;
   unsigned    flags;

   flags = DisableInterrupts();

   for( index = 0; index < GUSWAVE_MaxVoices; index++ )
      {
      if ( GUSWAVE_Voices[ index ].Active )
         {
         NumVoices++;
         }
      }

   RestoreInterrupts( flags );

   return( NumVoices );
   }


/*---------------------------------------------------------------------
   Function: GUSWAVE_Kill

   Stops output of the voice associated with the specified handle.
---------------------------------------------------------------------*/

static int GUSWAVE_Kill
   (
   int handle
   )

   {
   VoiceNode *voice;
   unsigned  flags;

   flags = DisableInterrupts();

   voice = GUSWAVE_GetVoice( handle );

   if ( voice == NULL )
      {
      RestoreInterrupts( flags );
      GUSWAVE_SetErrorCode( GUSWAVE_VoiceNotFound );
      return( GUSWAVE_Warning );
      }

   RestoreInterrupts( flags );

   if ( voice->Active )
      {
//FIXME      gf1_stop_digital( voice->GF1voice );
      }

   return( GUSWAVE_Ok );
   }


/*---------------------------------------------------------------------
   Function: GUSWAVE_KillAllVoices

   Stops output of all currently active voices.
---------------------------------------------------------------------*/

int GUSWAVE_KillAllVoices
   (
   void
   )

   {
   int i;
   unsigned  flags;

   if ( !GUSWAVE_Installed )
      {
      return( GUSWAVE_Ok );
      }

   flags = DisableInterrupts();

   // Remove all the voices from the list
   for( i = 0; i < GUSWAVE_MaxVoices; i++ )
      {
      if ( GUSWAVE_Voices[ i ].Active )
         {
//FIXME         gf1_stop_digital( GUSWAVE_Voices[ i ].GF1voice );
         }
      }

   for( i = 0; i < MAX_VOICES; i++ )
      {
      VoiceStatus[ i ].playing = FALSE;
      VoiceStatus[ i ].Voice   = NULL;
      }

   VoicePool.start = NULL;
   VoicePool.end   = NULL;
   VoiceList.start = NULL;
   VoiceList.end   = NULL;

   for( i = 0; i < GUSWAVE_MaxVoices; i++ )
      {
      GUSWAVE_Voices[ i ].Active = FALSE;
      if ( GUSWAVE_Voices[ i ].mem != 0 )
         {
         LL_AddToTail( VoiceNode, &VoicePool, &GUSWAVE_Voices[ i ] );
         }
      }

   RestoreInterrupts( flags );

   return( GUSWAVE_Ok );
   }


/*---------------------------------------------------------------------
   Function: GUSWAVE_SetVolume

   Sets the total volume of the digitized sounds.
---------------------------------------------------------------------*/

void GUSWAVE_SetVolume
   (
   int volume
   )

   {
   int i;

   volume = max( 0, volume );
   volume = min( 255, volume );
   GUSWAVE_Volume = MAX_VOLUME - ( 255 - volume ) * 4;

   for( i = 0; i < GUSWAVE_MaxVoices; i++ )
      {
      if ( GUSWAVE_Voices[ i ].Active )
         {
//FIXME         gf1_dig_set_vol( GUSWAVE_Voices[ i ].GF1voice, GUSWAVE_Volume - ( 255 - GUSWAVE_Voices[ i ].Volume ) * 4 );
         }
      }
   }


/*---------------------------------------------------------------------
   Function: GUSWAVE_AllocVoice

   Retrieve an inactive or lower priority voice for output.
---------------------------------------------------------------------*/

static VoiceNode *GUSWAVE_AllocVoice
   (
   int priority
   )

   {
   VoiceNode   *voice;
   VoiceNode   *node;
   unsigned    flags;

   // If we don't have any free voices, check if we have a higher
   // priority than one that is playing.
   if ( GUSWAVE_VoicesPlaying() >= GUSWAVE_MaxVoices )
      {
      flags = DisableInterrupts();

      node = VoiceList.start;
      voice = node;
      while( node != NULL )
         {
         if ( node->priority < voice->priority )
            {
            voice = node;
            }

         node = node->next;
         }

      RestoreInterrupts( flags );

      if ( priority >= voice->priority )
         {
         GUSWAVE_Kill( voice->handle );
         }
      }

   // Check if any voices are in the voice pool
   flags = DisableInterrupts();

   voice = VoicePool.start;
   if ( voice != NULL )
      {
      LL_Remove( VoiceNode, &VoicePool, voice );
      }

   RestoreInterrupts( flags );

   if ( voice != NULL )
      {
      do
         {
         GUSWAVE_VoiceHandle++;
         if ( GUSWAVE_VoiceHandle < GUSWAVE_MinVoiceHandle )
            {
            GUSWAVE_VoiceHandle = GUSWAVE_MinVoiceHandle;
            }
         }
      while( GUSWAVE_VoicePlaying( GUSWAVE_VoiceHandle ) );

      voice->handle = GUSWAVE_VoiceHandle;
      }

   return( voice );
   }


/*---------------------------------------------------------------------
   Function: GUSWAVE_GetNextVOCBlock

   Interperate the information of a VOC format sound file.
---------------------------------------------------------------------*/

static playbackstatus GUSWAVE_GetNextVOCBlock
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
      voice->sound       += MAX_BLOCK_LENGTH;
      voice->length       = min( voice->BlockLength, MAX_BLOCK_LENGTH );
      voice->BlockLength -= voice->length;
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
            voice->Playing = FALSE;
            done = TRUE;
            break;

         case 1 :
            // Sound data block
            voice->bits = 8;

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
            voice->LoopCount = *( unsigned short * )ptr;
            ptr += blocklength;
            voice->LoopStart = ptr;
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
            voice->bits = 8;
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
               ptr += 12;
               blocklength -= 12;
               voice->bits  = 8;
               done = TRUE;
               }
            else if ( ( BitsPerSample == 16 ) && ( Channels == 1 ) &&
               ( Format == VOC_16BIT ) )
               {
               ptr         += 12;
               blocklength -= 12;
               voice->bits  = 16;
               done         = TRUE;
               }
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
      voice->RateScale    = ( voice->SamplingRate * voice->PitchScale ) >> 16;

      voice->length       = min( blocklength, MAX_BLOCK_LENGTH );
      voice->BlockLength  = blocklength - voice->length;

      return( KeepPlaying );
      }

   return( SoundDone );
   }


/*---------------------------------------------------------------------
   Function: GUSWAVE_GetNextDemandFeedBlock

   Controls playback of demand fed data.
---------------------------------------------------------------------*/

static playbackstatus GUSWAVE_GetNextDemandFeedBlock
   (
   VoiceNode *voice
   )

   {
   if ( voice->BlockLength > 0 )
      {
      voice->sound       += voice->length;
      voice->length       = min( voice->BlockLength, 0x8000 );
      voice->BlockLength -= voice->length;

      return( KeepPlaying );
      }

   if ( voice->DemandFeed == NULL )
      {
      return( SoundDone );
      }

   ( voice->DemandFeed )( &voice->sound, &voice->BlockLength );
//   voice->sound = GUS_Silence16;
//   voice->BlockLength = 256;

   voice->length       = min( voice->BlockLength, 0x8000 );
   voice->BlockLength -= voice->length;

   if ( ( voice->length > 0 ) && ( voice->sound != NULL ) )
      {
      return( KeepPlaying );
      }
   return( NoMoreData );
   }


/*---------------------------------------------------------------------
   Function: GUSWAVE_Play

   Begins playback of digitized sound.
---------------------------------------------------------------------*/

static int GUSWAVE_Play
   (
   VoiceNode *voice,
   int angle,
   int volume,
   int channels
   )

   {
   int      VoiceNumber;
   int      type;
   int      pan;
   unsigned flags;
   int ( *servicefunction )( int reason, int voice, unsigned char **buf, unsigned long *size );

   type = 0;
   if ( channels != 1 )
      {
      type |= TYPE_STEREO;
      }

   if ( voice->bits == 8 )
      {
      type |= TYPE_8BIT;
      type |= TYPE_INVERT_MSB;
      }

   voice->GF1voice  = -1;

   angle &= 31;
   pan = GUSWAVE_PanTable[ angle ];
   if ( GUSWAVE_SwapLeftRight )
      {
      pan = 15 - pan;
      }

   voice->Pan = pan;

   volume = max( 0, volume );
   volume = min( 255, volume );
   voice->Volume = volume;

   servicefunction = GUSWAVE_CallBack;

   VoiceNumber = NO_MORE_VOICES; //FIXMEgf1_play_digital( 0, voice->sound, voice->length, voice->mem, GUSWAVE_Volume - ( 255 - volume ) * 4, pan, voice->RateScale, type, &GUS_HoldBuffer, servicefunction );

   if ( VoiceNumber == NO_MORE_VOICES )
      {
      flags = DisableInterrupts();
      LL_AddToTail( VoiceNode, &VoicePool, voice );
      RestoreInterrupts( flags );

      GUSWAVE_SetErrorCode( GUSWAVE_NoVoices );
      return( GUSWAVE_Warning );
      }

   flags = DisableInterrupts();
   voice->GF1voice = VoiceNumber;
   voice->Active   = TRUE;
   LL_AddToTail( VoiceNode, &VoiceList, voice );
   VoiceStatus[ VoiceNumber ].playing = TRUE;
   VoiceStatus[ VoiceNumber ].Voice   = voice;

   RestoreInterrupts( flags );

   return( voice->handle );
   }


/*---------------------------------------------------------------------
   Function: GUSWAVE_StartDemandFeedPlayback

   Begins playback of digitized sound.
---------------------------------------------------------------------*/

int GUSWAVE_StartDemandFeedPlayback
   (
   void ( *function )( char **ptr, unsigned long *length ),
   int   channels,
   int   bits,
   int   rate,
   int   pitchoffset,
   int   angle,
   int   volume,
   int   priority,
   unsigned long callbackval
   )

   {
   VoiceNode *voice;
   int        handle;

   // Request a voice from the voice pool
   voice = GUSWAVE_AllocVoice( priority );
   if ( voice == NULL )
      {
      GUSWAVE_SetErrorCode( GUSWAVE_NoVoices );
      return( GUSWAVE_Warning );
      }

   voice->wavetype    = DemandFeed;
   voice->bits        = bits;
//FIXME   voice->GetSound    = GUSWAVE_GetNextDemandFeedBlock;
   voice->Playing     = TRUE;
   voice->DemandFeed  = function;
   voice->LoopStart   = NULL;
   voice->LoopCount   = 0;
   voice->BlockLength = 0;
   voice->length      = 256;
   voice->sound       = ( bits == 8 ) ? GUS_Silence8 : ( unsigned char * )GUS_Silence16;
   voice->NextBlock   = NULL;
   voice->next        = NULL;
   voice->prev        = NULL;
   voice->priority    = priority;
   voice->callbackval = callbackval;
   voice->PitchScale  = PITCH_GetScale( pitchoffset );
   voice->SamplingRate = rate;
   voice->RateScale   = ( voice->SamplingRate * voice->PitchScale ) >> 16;

   handle = GUSWAVE_Play( voice, angle, volume, channels );

   return( handle );
   }


/*---------------------------------------------------------------------
   Function: GUSWAVE_SetReverseStereo

   Set the orientation of the left and right channels.
---------------------------------------------------------------------*/

static void GUSWAVE_SetReverseStereo
   (
   int setting
   )

   {
   GUSWAVE_SwapLeftRight = setting;
   }


/*---------------------------------------------------------------------
   Function: GUSWAVE_InitVoices

   Begins playback of digitized sound.
---------------------------------------------------------------------*/

static int GUSWAVE_InitVoices
   (
   void
   )

   {
   int i;

   for( i = 0; i < MAX_VOICES; i++ )
      {
      VoiceStatus[ i ].playing = FALSE;
      VoiceStatus[ i ].Voice   = NULL;
      }

   VoicePool.start = NULL;
   VoicePool.end   = NULL;
   VoiceList.start = NULL;
   VoiceList.end   = NULL;

   for( i = 0; i < VOICES; i++ )
      {
      GUSWAVE_Voices[ i ].num      = -1;
      GUSWAVE_Voices[ i ].Active   = FALSE;
      GUSWAVE_Voices[ i ].GF1voice = -1;
      GUSWAVE_Voices[ i ].mem      = NULL;
      }

   for( i = 0; i < VOICES; i++ )
      {
      GUSWAVE_Voices[ i ].num      = i;
      GUSWAVE_Voices[ i ].Active   = FALSE;
      GUSWAVE_Voices[ i ].GF1voice = 0;

      GUSWAVE_Voices[ i ].mem = 0; //FIXME gf1_malloc( GF1BSIZE );
      if ( GUSWAVE_Voices[ i ].mem == 0 )
         {
         GUSWAVE_MaxVoices = i;
         if ( i < 1 )
            {
            if ( GUSMIDI_Installed )
               {
               GUSWAVE_SetErrorCode( GUSWAVE_UltraNoMemMIDI );
               }
            else
               {
               GUSWAVE_SetErrorCode( GUSWAVE_UltraNoMem );
               }
            return( GUSWAVE_Error );
            }

         break;
         }

      LL_AddToTail( VoiceNode, &VoicePool, &GUSWAVE_Voices[ i ] );
      }

   return( GUSWAVE_Ok );
   }


/*---------------------------------------------------------------------
   Function: GUSWAVE_Init

   Initializes the Gravis Ultrasound for digitized sound playback.
---------------------------------------------------------------------*/

int GUSWAVE_Init
   (
   int numvoices
   )

   {
   int status;

   if ( GUSWAVE_Installed )
      {
      GUSWAVE_Shutdown();
      }

   GUSWAVE_SetErrorCode( GUSWAVE_Ok );

   status = GUS_Init();
   if ( status != GUS_Ok )
      {
      GUSWAVE_SetErrorCode( GUSWAVE_GUSError );
      return( GUSWAVE_Error );
      }

   GUSWAVE_MaxVoices = min( numvoices, VOICES );
   GUSWAVE_MaxVoices = max( GUSWAVE_MaxVoices, 0 );

   status = GUSWAVE_InitVoices();
   if ( status != GUSWAVE_Ok )
      {
      GUS_Shutdown();
      return( status );
      }

   GUSWAVE_SetReverseStereo( FALSE );

   GUSWAVE_CallBackFunc = NULL;
   GUSWAVE_Installed = TRUE;

   return( GUSWAVE_Ok );
   }
