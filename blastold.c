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
#include "dma.h"
#include "irq.h"
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


#define USESTACK

static const int BLASTER_Interrupts[ BLASTER_MaxIrq + 1 ]  =
   {
   INVALID, INVALID,     0xa,     0xb,
   INVALID,     0xd, INVALID,     0xf,
   INVALID, INVALID,    0x72,    0x73,
      0x74, INVALID, INVALID,    0x77
   };

static const int BLASTER_SampleSize[ BLASTER_MaxMixMode + 1 ] =
   {
   MONO_8BIT_SAMPLE_SIZE,  STEREO_8BIT_SAMPLE_SIZE,
   MONO_16BIT_SAMPLE_SIZE, STEREO_16BIT_SAMPLE_SIZE
   };

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

static void    ( __interrupt __far *BLASTER_OldInt )( void );

BLASTER_CONFIG BLASTER_Config =
   {
   UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED
   };

static int BLASTER_Installed = FALSE;
static int BLASTER_Version;

static char   *BLASTER_DMABuffer;
static char   *BLASTER_DMABufferEnd;
static char   *BLASTER_CurrentDMABuffer;
static int     BLASTER_TotalDMABufferSize;

static int      BLASTER_TransferLength   = 0;
static int      BLASTER_MixMode          = BLASTER_DefaultMixMode;
static int      BLASTER_SamplePacketSize = MONO_16BIT_SAMPLE_SIZE;
static unsigned BLASTER_SampleRate       = BLASTER_DefaultSampleRate;

static unsigned BLASTER_HaltTransferCommand = DSP_Halt8bitTransfer;

static volatile int   BLASTER_SoundPlaying;
static volatile int   BLASTER_SoundRecording;

static void ( *BLASTER_CallBack )( void );

static int  BLASTER_IntController1Mask;
static int  BLASTER_IntController2Mask;

static int BLASTER_MixerAddress = UNDEFINED;
static int BLASTER_MixerType    = 0;
static int BLASTER_OriginalMidiVolumeLeft   = 255;
static int BLASTER_OriginalMidiVolumeRight  = 255;
static int BLASTER_OriginalVoiceVolumeLeft  = 255;
static int BLASTER_OriginalVoiceVolumeRight = 255;

static int BLASTER_WaveBlasterPort  = UNDEFINED;
static int BLASTER_WaveBlasterState = 0x0F;

// adequate stack size
#define kStackSize 2048

static unsigned short StackSelector = NULL;
static unsigned long  StackPointer;

static unsigned short oldStackSelector;
static unsigned long  oldStackPointer;

// This is defined because we can't create local variables in a
// function that switches stacks.
static int GlobalStatus;

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

static int BLASTER_ErrorCode = BLASTER_Ok;

#define BLASTER_SetErrorCode( status ) \
   BLASTER_ErrorCode   = ( status );


/*---------------------------------------------------------------------
   Function: BLASTER_EnableInterrupt

   Enables the triggering of the sound card interrupt.
---------------------------------------------------------------------*/

static void BLASTER_EnableInterrupt
   (
   void
   )

   {
   int Irq;
   int mask;

   // Unmask system interrupt
   Irq  = BLASTER_Config.Interrupt;
   if ( Irq < 8 )
      {
      mask = inp( 0x21 ) & ~( 1 << Irq );
      outp( 0x21, mask  );
      }
   else
      {
      mask = inp( 0xA1 ) & ~( 1 << ( Irq - 8 ) );
      outp( 0xA1, mask  );

      mask = inp( 0x21 ) & ~( 1 << 2 );
      outp( 0x21, mask  );
      }

   }


/*---------------------------------------------------------------------
   Function: BLASTER_DisableInterrupt

   Disables the triggering of the sound card interrupt.
---------------------------------------------------------------------*/

static void BLASTER_DisableInterrupt
   (
   void
   )

   {
   int Irq;
   int mask;

   // Restore interrupt mask
   Irq  = BLASTER_Config.Interrupt;
   if ( Irq < 8 )
      {
      mask  = inp( 0x21 ) & ~( 1 << Irq );
      mask |= BLASTER_IntController1Mask & ( 1 << Irq );
      outp( 0x21, mask  );
      }
   else
      {
      mask  = inp( 0x21 ) & ~( 1 << 2 );
      mask |= BLASTER_IntController1Mask & ( 1 << 2 );
      outp( 0x21, mask  );

      mask  = inp( 0xA1 ) & ~( 1 << ( Irq - 8 ) );
      mask |= BLASTER_IntController2Mask & ( 1 << ( Irq - 8 ) );
      outp( 0xA1, mask  );
      }
   }


/*---------------------------------------------------------------------
   Function: BLASTER_ServiceInterrupt

   Handles interrupt generated by sound card at the end of a voice
   transfer.  Calls the user supplied callback function.
---------------------------------------------------------------------*/

static void __interrupt __far BLASTER_ServiceInterrupt
   (
   void
   )

   {
   #ifdef USESTACK
   // save stack
   GetStack( &oldStackSelector, &oldStackPointer );

   // set our stack
   SetStack( StackSelector, StackPointer );
   #endif

   // Acknowledge interrupt
   // Check if this is this an SB16 or newer
   if ( BLASTER_Version >= DSP_Version4xx )
      {
      outp( BLASTER_Config.Address + BLASTER_MixerAddressPort,
         MIXER_DSP4xxISR_Ack );

      GlobalStatus = inp( BLASTER_Config.Address + BLASTER_MixerDataPort );

      // Check if a 16-bit DMA interrupt occurred
      if ( GlobalStatus & MIXER_16BITDMA_INT )
         {
         // Acknowledge 16-bit transfer interrupt
         inp( BLASTER_Config.Address + BLASTER_16BitDMAAck );
         }
      else if ( GlobalStatus & MIXER_8BITDMA_INT )
         {
         inp( BLASTER_Config.Address + BLASTER_DataAvailablePort );
         }
      else
         {
         #ifdef USESTACK
         // restore stack
         SetStack( oldStackSelector, oldStackPointer );
         #endif

         // Wasn't our interrupt.  Call the old one.
         _chain_intr( BLASTER_OldInt );
         }
      }
   else
      {
      // Older card - can't detect if an interrupt occurred.
      inp( BLASTER_Config.Address + BLASTER_DataAvailablePort );
      }

   // Keep track of current buffer
   BLASTER_CurrentDMABuffer += BLASTER_TransferLength;

   if ( BLASTER_CurrentDMABuffer >= BLASTER_DMABufferEnd )
      {
      BLASTER_CurrentDMABuffer = BLASTER_DMABuffer;
      }

   // Continue playback on cards without autoinit mode
   if ( BLASTER_Version < DSP_Version2xx )
      {
      if ( BLASTER_SoundPlaying )
         {
         BLASTER_DSP1xx_BeginPlayback( BLASTER_TransferLength );
         }

      if ( BLASTER_SoundRecording )
         {
         BLASTER_DSP1xx_BeginRecord( BLASTER_TransferLength );
         }
      }

   // Call the caller's callback function
   if ( BLASTER_CallBack != NULL )
      {
      BLASTER_CallBack();
      }

   #ifdef USESTACK
   // restore stack
   SetStack( oldStackSelector, oldStackPointer );
   #endif

   // send EOI to Interrupt Controller
   if ( BLASTER_Config.Interrupt > 7 )
      {
      outp( 0xA0, 0x20 );
      }

   outp( 0x20, 0x20 );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_WriteDSP

   Writes a byte of data to the sound card's DSP.
---------------------------------------------------------------------*/

static int BLASTER_WriteDSP
   (
   unsigned data
   )

   {
   int      port;
   unsigned count;
   int      status;

   port = BLASTER_Config.Address + BLASTER_WritePort;

   status = BLASTER_Error;

   count = 0xFFFF;

   do
      {
      if ( ( inp( port ) & 0x80 ) == 0 )
         {
         outp( port, data );
         status = BLASTER_Ok;
         break;
         }

      count--;
      }
   while( count > 0 );

   if ( status != BLASTER_Ok )
      {
      BLASTER_SetErrorCode( BLASTER_CardNotReady );
      }

   return( status );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_ReadDSP

   Reads a byte of data from the sound card's DSP.
---------------------------------------------------------------------*/

static int BLASTER_ReadDSP
   (
   void
   )

   {
   int      port;
   unsigned count;
   int      status;

   port = BLASTER_Config.Address + BLASTER_DataAvailablePort;

   status = BLASTER_Error;

   count = 0xFFFF;

   do
      {
      if ( inp( port ) & 0x80 )
         {
         status = inp( BLASTER_Config.Address + BLASTER_ReadPort );
         break;
         }

      count--;
      }
   while( count > 0 );

   if ( status == BLASTER_Error )
      {
      BLASTER_SetErrorCode( BLASTER_CardNotReady );
      }

   return( status );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_ResetDSP

   Sends a reset command to the sound card's Digital Signal Processor
   (DSP), causing it to perform an initialization.
---------------------------------------------------------------------*/

static int BLASTER_ResetDSP
   (
   void
   )

   {
   volatile int count;
   int port;
   int status;

   port = BLASTER_Config.Address + BLASTER_ResetPort;

   status = BLASTER_CardNotReady;

   outp( port, 1 );

   count = 0x100;
   do
      {
      count--;
      }
   while( count > 0 );

   outp( port, 0 );

   count = 100;

   do
      {
      if ( BLASTER_ReadDSP() == BLASTER_Ready )
         {
         status = BLASTER_Ok;
         break;
         }

      count--;
      }
   while( count > 0 );

   return( status );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_GetDSPVersion

   Returns the version number of the sound card's DSP.
---------------------------------------------------------------------*/

static int BLASTER_GetDSPVersion
   (
   void
   )

   {
   int MajorVersion;
   int MinorVersion;
   int version;

   BLASTER_WriteDSP( DSP_GetVersion );

   MajorVersion   = BLASTER_ReadDSP();
   MinorVersion   = BLASTER_ReadDSP();

   if ( ( MajorVersion == BLASTER_Error ) ||
      ( MinorVersion == BLASTER_Error ) )
      {
      BLASTER_SetErrorCode( BLASTER_CardNotReady );
      return( BLASTER_Error );
      }

   version = ( MajorVersion << 8 ) + MinorVersion;

   return( version );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_SpeakerOn

   Enables output from the DAC.
---------------------------------------------------------------------*/

static void BLASTER_SpeakerOn
   (
   void
   )

   {
   BLASTER_WriteDSP( DSP_SpeakerOn );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_SpeakerOff

   Disables output from the DAC.
---------------------------------------------------------------------*/

static void BLASTER_SpeakerOff
   (
   void
   )

   {
   BLASTER_WriteDSP( DSP_SpeakerOff );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_SetPlaybackRate

   Sets the rate at which the digitized sound will be played in
   hertz.
---------------------------------------------------------------------*/

static void BLASTER_SetPlaybackRate
   (
   unsigned rate
   )

   {
   int LoByte;
   int HiByte;
   CARD_CAPABILITY const *card;

   card = &BLASTER_CardConfig[ BLASTER_Config.Type ];

   if ( BLASTER_Version < DSP_Version4xx )
      {
      int  timeconstant;
      long ActualRate;

      // Send sampling rate as time constant for older Sound
      // Blaster compatible cards.

      ActualRate = rate * BLASTER_SamplePacketSize;
      if ( ActualRate < card->MinSamplingRate )
         {
         rate = card->MinSamplingRate / BLASTER_SamplePacketSize;
         }

      if ( ActualRate > card->MaxSamplingRate )
         {
         rate = card->MaxSamplingRate / BLASTER_SamplePacketSize;
         }

      timeconstant = ( int )CalcTimeConstant( rate, BLASTER_SamplePacketSize );

      // Keep track of what the actual rate is
      BLASTER_SampleRate  = ( unsigned )CalcSamplingRate( timeconstant );
      BLASTER_SampleRate /= BLASTER_SamplePacketSize;

      BLASTER_WriteDSP( DSP_SetTimeConstant );
      BLASTER_WriteDSP( timeconstant );
      }
   else
      {
      // Send literal sampling rate for cards with DSP version
      // 4.xx (Sound Blaster 16)

      BLASTER_SampleRate = rate;

      if ( BLASTER_SampleRate < card->MinSamplingRate )
         {
         BLASTER_SampleRate = card->MinSamplingRate;
         }

      if ( BLASTER_SampleRate > card->MaxSamplingRate )
         {
         BLASTER_SampleRate = card->MaxSamplingRate;
         }

      HiByte = hibyte( BLASTER_SampleRate );
      LoByte = lobyte( BLASTER_SampleRate );

      // Set playback rate
      BLASTER_WriteDSP( DSP_Set_DA_Rate );
      BLASTER_WriteDSP( HiByte );
      BLASTER_WriteDSP( LoByte );

      // Set recording rate
      BLASTER_WriteDSP( DSP_Set_AD_Rate );
      BLASTER_WriteDSP( HiByte );
      BLASTER_WriteDSP( LoByte );
      }
   }


/*---------------------------------------------------------------------
   Function: BLASTER_GetPlaybackRate

   Returns the rate at which the digitized sound will be played in
   hertz.
---------------------------------------------------------------------*/

unsigned BLASTER_GetPlaybackRate
   (
   void
   )

   {
   return( BLASTER_SampleRate );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_SetMixMode

   Sets the sound card to play samples in mono or stereo.
---------------------------------------------------------------------*/

int BLASTER_SetMixMode
   (
   int mode
   )

   {
   int   port;
   int   data;
   int   CardType;

   CardType = BLASTER_Config.Type;

   mode &= BLASTER_MaxMixMode;

   if ( !( BLASTER_CardConfig[ CardType ].MaxMixMode & STEREO ) )
      {
      mode &= ~STEREO;
      }

   if ( !( BLASTER_CardConfig[ CardType ].MaxMixMode & SIXTEEN_BIT ) )
      {
      mode &= ~SIXTEEN_BIT;
      }

   BLASTER_MixMode = mode;
   BLASTER_SamplePacketSize = BLASTER_SampleSize[ mode ];

   // For the Sound Blaster Pro, we have to set the mixer chip
   // to play mono or stereo samples.

   if ( ( CardType == SBPro ) || ( CardType == SBPro2 ) )
      {
      port = BLASTER_Config.Address + BLASTER_MixerAddressPort;
      outp( port, MIXER_SBProOutputSetting );

      port = BLASTER_Config.Address + BLASTER_MixerDataPort;

      // Get current mode
      data = inp( port );

      // set stereo mode bit
      if ( mode & STEREO )
         {
         data |= MIXER_SBProStereoFlag;
         }
      else
         {
         data &= ~MIXER_SBProStereoFlag;
         }

      // set the mode
      outp( port, data );

      BLASTER_SetPlaybackRate( BLASTER_SampleRate );
      }

   return( mode );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_StopPlayback

   Ends the DMA transfer of digitized sound to the sound card.
---------------------------------------------------------------------*/

void BLASTER_StopPlayback
   (
   void
   )

   {
   int DmaChannel;

   // Don't allow anymore interrupts
   BLASTER_DisableInterrupt();

   if ( BLASTER_HaltTransferCommand == DSP_Reset )
      {
      BLASTER_ResetDSP();
      }
   else
      {
      BLASTER_WriteDSP( BLASTER_HaltTransferCommand );
      }

   // Disable the DMA channel
   if ( BLASTER_MixMode & SIXTEEN_BIT )
      {
      DmaChannel = BLASTER_Config.Dma16;
      }
   else
      {
      DmaChannel = BLASTER_Config.Dma8;
      }
   DMA_EndTransfer( DmaChannel );

   // Turn off speaker
   BLASTER_SpeakerOff();

   BLASTER_SoundPlaying = FALSE;
   BLASTER_SoundRecording = FALSE;

   BLASTER_DMABuffer = NULL;
   }


/*---------------------------------------------------------------------
   Function: BLASTER_SetupDMABuffer

   Programs the DMAC for sound transfer.
---------------------------------------------------------------------*/

static int BLASTER_SetupDMABuffer
   (
   char *BufferPtr,
   int   BufferSize,
   int   mode
   )

   {
   int DmaChannel;
   int DmaStatus;
   int errorcode;

   if ( BLASTER_MixMode & SIXTEEN_BIT )
      {
      DmaChannel = BLASTER_Config.Dma16;
      errorcode  = BLASTER_DMA16NotSet;
      }
   else
      {
      DmaChannel = BLASTER_Config.Dma8;
      errorcode  = BLASTER_DMANotSet;
      }

   if ( DmaChannel == UNDEFINED )
      {
      BLASTER_SetErrorCode( errorcode );
      return( BLASTER_Error );
      }

   DmaStatus = DMA_SetupTransfer( DmaChannel, BufferPtr, BufferSize, mode );
   if ( DmaStatus == DMA_Error )
      {
      BLASTER_SetErrorCode( BLASTER_DmaError );
      return( BLASTER_Error );
      }

   BLASTER_DMAChannel = DmaChannel;

   BLASTER_DMABuffer          = BufferPtr;
   BLASTER_CurrentDMABuffer   = BufferPtr;
   BLASTER_TotalDMABufferSize = BufferSize;
   BLASTER_DMABufferEnd       = BufferPtr + BufferSize;

   return( BLASTER_Ok );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_GetCurrentPos

   Returns the offset within the current sound being played.
---------------------------------------------------------------------*/

int BLASTER_GetCurrentPos
   (
   void
   )

   {
   char *CurrentAddr;
   int   DmaChannel;
   int   offset;

   if ( !BLASTER_SoundPlaying )
      {
      BLASTER_SetErrorCode( BLASTER_NoSoundPlaying );
      return( BLASTER_Error );
      }

   if ( BLASTER_MixMode & SIXTEEN_BIT )
      {
      DmaChannel = BLASTER_Config.Dma16;
      }
   else
      {
      DmaChannel = BLASTER_Config.Dma8;
      }

   if ( DmaChannel == UNDEFINED )
      {
      BLASTER_SetErrorCode( BLASTER_DMANotSet );
      return( BLASTER_Error );
      }

   CurrentAddr = DMA_GetCurrentPos( DmaChannel );

   offset = ( int )( ( ( unsigned long )CurrentAddr ) -
      ( ( unsigned long )BLASTER_CurrentDMABuffer ) );

   if ( BLASTER_MixMode & SIXTEEN_BIT )
      {
      offset >>= 1;
      }

   if ( BLASTER_MixMode & STEREO )
      {
      offset >>= 1;
      }

   return( offset );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_DSP1xx_BeginPlayback

   Starts playback of digitized sound on cards compatible with DSP
   version 1.xx.
---------------------------------------------------------------------*/

static int BLASTER_DSP1xx_BeginPlayback
   (
   int length
   )

   {
   int SampleLength;
   int LoByte;
   int HiByte;

   SampleLength = length - 1;
   HiByte = hibyte( SampleLength );
   LoByte = lobyte( SampleLength );

   // Program DSP to play sound
   BLASTER_WriteDSP( DSP_Old8BitDAC );
   BLASTER_WriteDSP( LoByte );
   BLASTER_WriteDSP( HiByte );

   BLASTER_HaltTransferCommand = DSP_Halt8bitTransfer;

   BLASTER_SoundPlaying = TRUE;

   return( BLASTER_Ok );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_DSP2xx_BeginPlayback

   Starts playback of digitized sound on cards compatible with DSP
   version 2.xx.
---------------------------------------------------------------------*/

static int BLASTER_DSP2xx_BeginPlayback
   (
   int length
   )

   {
   int SampleLength;
   int LoByte;
   int HiByte;

   SampleLength = length - 1;
   HiByte = hibyte( SampleLength );
   LoByte = lobyte( SampleLength );

   BLASTER_WriteDSP( DSP_SetBlockLength );
   BLASTER_WriteDSP( LoByte );
   BLASTER_WriteDSP( HiByte );

   if ( ( BLASTER_Version >= DSP_Version201 ) && ( DSP_MaxNormalRate <
      ( BLASTER_SampleRate * BLASTER_SamplePacketSize ) ) )
      {
      BLASTER_WriteDSP( DSP_8BitHighSpeedAutoInitMode );
      BLASTER_HaltTransferCommand = DSP_Reset;
      }
   else
      {
      BLASTER_WriteDSP( DSP_8BitAutoInitMode );
      BLASTER_HaltTransferCommand = DSP_Halt8bitTransfer;
      }

   BLASTER_SoundPlaying = TRUE;

   return( BLASTER_Ok );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_DSP4xx_BeginPlayback

   Starts playback of digitized sound on cards compatible with DSP
   version 4.xx, such as the Sound Blaster 16.
---------------------------------------------------------------------*/

static int BLASTER_DSP4xx_BeginPlayback
   (
   int length
   )

   {
   int TransferCommand;
   int TransferMode;
   int SampleLength;
   int LoByte;
   int HiByte;

   if ( BLASTER_MixMode & SIXTEEN_BIT )
      {
      TransferCommand = DSP_16BitDAC;
      SampleLength = ( length / 2 ) - 1;
      BLASTER_HaltTransferCommand = DSP_Halt16bitTransfer;
      if ( BLASTER_MixMode & STEREO )
         {
         TransferMode = DSP_SignedStereoData;
         }
      else
         {
         TransferMode = DSP_SignedMonoData;
         }
      }
   else
      {
      TransferCommand = DSP_8BitDAC;
      SampleLength = length - 1;
      BLASTER_HaltTransferCommand = DSP_Halt8bitTransfer;
      if ( BLASTER_MixMode & STEREO )
         {
         TransferMode = DSP_UnsignedStereoData;
         }
      else
         {
         TransferMode = DSP_UnsignedMonoData;
         }
      }

   HiByte = hibyte( SampleLength );
   LoByte = lobyte( SampleLength );

   // Program DSP to play sound
   BLASTER_WriteDSP( TransferCommand );
   BLASTER_WriteDSP( TransferMode );
   BLASTER_WriteDSP( LoByte );
   BLASTER_WriteDSP( HiByte );

   BLASTER_SoundPlaying = TRUE;

   return( BLASTER_Ok );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_BeginBufferedPlayback

   Begins multibuffered playback of digitized sound on the sound card.
---------------------------------------------------------------------*/

int BLASTER_BeginBufferedPlayback
   (
   char    *BufferStart,
   int      BufferSize,
   int      NumDivisions,
   unsigned SampleRate,
   int      MixMode,
   void  ( *CallBackFunc )( void )
   )

   {
   int DmaStatus;
   int TransferLength;

   // VERSIONS RESTORATION
   // Do the check, based on version
//JIM
#if (LIBVER_ASSREV < 19960116L)
   if ( BLASTER_SoundPlaying || BLASTER_SoundRecording )
#endif
      {
      BLASTER_StopPlayback();
      }

   BLASTER_SetMixMode( MixMode );

   DmaStatus = BLASTER_SetupDMABuffer( BufferStart, BufferSize, DMA_AutoInitRead );
   if ( DmaStatus == BLASTER_Error )
      {
      return( BLASTER_Error );
      }

   BLASTER_SetPlaybackRate( SampleRate );

   BLASTER_SetCallBack( CallBackFunc );

   BLASTER_EnableInterrupt();

   // Turn on speaker
   BLASTER_SpeakerOn();

   TransferLength = BufferSize / NumDivisions;
   BLASTER_TransferLength = TransferLength;

   //  Program the sound card to start the transfer.
   if ( BLASTER_Version < DSP_Version2xx )
      {
      BLASTER_DSP1xx_BeginPlayback( TransferLength );
      }
   else if ( BLASTER_Version < DSP_Version4xx )
      {
      BLASTER_DSP2xx_BeginPlayback( TransferLength );
      }
   else
      {
      BLASTER_DSP4xx_BeginPlayback( TransferLength );
      }

   return( BLASTER_Ok );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_DSP1xx_BeginRecord

   Starts recording of digitized sound on cards compatible with DSP
   version 1.xx.
---------------------------------------------------------------------*/

static int BLASTER_DSP1xx_BeginRecord
   (
   int length
   )

   {
   int SampleLength;
   int LoByte;
   int HiByte;

   SampleLength = length - 1;
   HiByte = hibyte( SampleLength );
   LoByte = lobyte( SampleLength );

   // Program DSP to play sound
   BLASTER_WriteDSP( DSP_Old8BitADC );
   BLASTER_WriteDSP( LoByte );
   BLASTER_WriteDSP( HiByte );

   BLASTER_HaltTransferCommand = DSP_Halt8bitTransfer;

   BLASTER_SoundRecording = TRUE;

   return( BLASTER_Ok );
   }


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
   Function: BLASTER_SetVoiceVolume

   Sets the volume of the digitized sound channel on the Sound
   Blaster's mixer chip.
---------------------------------------------------------------------*/

int BLASTER_SetVoiceVolume
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
         BLASTER_SetErrorCode( BLASTER_NoMixer );
         status = BLASTER_Error;
      }

   return( status );
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
   Function: BLASTER_SaveVoiceVolume

   Saves the user's voice mixer settings.
---------------------------------------------------------------------*/

static void BLASTER_SaveVoiceVolume
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
            BLASTER_OriginalVoiceVolumeLeft =
               BLASTER_ReadMixer( MIXER_SBProVoice );
            break;

         case SB16 :
            BLASTER_OriginalVoiceVolumeLeft =
               BLASTER_ReadMixer( MIXER_SB16VoiceLeft );
            BLASTER_OriginalVoiceVolumeRight =
               BLASTER_ReadMixer( MIXER_SB16VoiceRight );
            break;
         }
      }
   }


/*---------------------------------------------------------------------
   Function: BLASTER_RestoreVoiceVolume

   Restores the user's voice mixer settings.
---------------------------------------------------------------------*/

static void BLASTER_RestoreVoiceVolume
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
            BLASTER_WriteMixer( MIXER_SBProVoice,
               BLASTER_OriginalVoiceVolumeLeft );
            break;

         case SB16 :
            BLASTER_WriteMixer( MIXER_SB16VoiceLeft,
               BLASTER_OriginalVoiceVolumeLeft );
            BLASTER_WriteMixer( MIXER_SB16VoiceRight,
               BLASTER_OriginalVoiceVolumeRight );
            break;
         }
      }
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
      BLASTER_SetErrorCode( errorcode );
      }

   return( status );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_SetCardSettings

   Sets up the sound card's parameters.
---------------------------------------------------------------------*/

int BLASTER_SetCardSettings
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
      BLASTER_SetErrorCode( BLASTER_UnsupportedCardType );
      return( BLASTER_Error );
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
  // *** VERSIONS RESTORATION ***
#if (LIBVER_ASSREV < 19950821L)
if ( ( Config.Type >= BLASTER_MinCardType ) ||
   ( Config.Type <= BLASTER_MaxCardType ) )
#endif
   BLASTER_MixerType = Config.Type;

   return( BLASTER_Ok );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_GetCardSettings

   Sets up the sound card's parameters.
---------------------------------------------------------------------*/

int BLASTER_GetCardSettings
   (
   BLASTER_CONFIG *Config
   )

   {
   if ( BLASTER_Config.Address == UNDEFINED )
      {
      return( BLASTER_Warning );
      }
   else
      {
      Config->Address   = BLASTER_Config.Address;
      Config->Type      = BLASTER_Config.Type;
      Config->Interrupt = BLASTER_Config.Interrupt;
      Config->Dma8      = BLASTER_Config.Dma8;
      Config->Dma16     = BLASTER_Config.Dma16;
      Config->Midi      = BLASTER_Config.Midi;
      Config->Emu       = BLASTER_Config.Emu;
      }

   return( BLASTER_Ok );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_GetCardInfo

   Returns the maximum number of bits that can represent a sample
   (8 or 16) and the number of channels (1 for mono, 2 for stereo).
---------------------------------------------------------------------*/

int BLASTER_GetCardInfo
   (
   int *MaxSampleBits,
   int *MaxChannels
   )

   {
   int CardType;

   CardType = BLASTER_Config.Type;

   if ( CardType == UNDEFINED )
      {
      BLASTER_SetErrorCode( BLASTER_CardTypeNotSet );
      return( BLASTER_Error );
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

   return( BLASTER_Ok );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_SetCallBack

   Specifies the user function to call at the end of a sound transfer.
---------------------------------------------------------------------*/

static void BLASTER_SetCallBack
   (
   void ( *func )( void )
   )

   {
   BLASTER_CallBack = func;
   }


/*---------------------------------------------------------------------
   Function: allocateTimerStack

   Allocate a block of memory from conventional (low) memory and return
   the selector (which can go directly into a segment register) of the
   memory block or 0 if an error occured.
---------------------------------------------------------------------*/

static unsigned short allocateTimerStack
   (
   unsigned short size
   )

   {
   union REGS regs;

   // clear all registers
   memset( &regs, 0, sizeof( regs ) );

   // DPMI allocate conventional memory
   regs.w.ax = 0x100;

   // size in paragraphs
   regs.w.bx = ( size + 15 ) / 16;

   int386( 0x31, &regs, &regs );
   if (!regs.w.cflag)
      {
      // DPMI call returns selector in dx
      // (ax contains real mode segment
      // which is ignored here)

      return( regs.w.dx );
      }

   // Couldn't allocate memory.
   return( NULL );
   }


/*---------------------------------------------------------------------
   Function: deallocateTimerStack

   Deallocate a block of conventional (low) memory given a selector to
   it.  Assumes the block was allocated with DPMI function 0x100.
---------------------------------------------------------------------*/

static void deallocateTimerStack
   (
   unsigned short selector
   )

   {
   union REGS regs;

   if ( selector != NULL )
      {
      // clear all registers
      memset( &regs, 0, sizeof( regs ) );

      regs.w.ax = 0x101;
      regs.w.dx = selector;
      int386( 0x31, &regs, &regs );
      }
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
            BLASTER_SetErrorCode( BLASTER_DmaError );
            return( BLASTER_Error );
            }
         }

      if ( BLASTER_Config.Dma8 != UNDEFINED )
         {
         status = DMA_VerifyChannel( BLASTER_Config.Dma8 );
         if ( status == DMA_Error )
            {
            BLASTER_SetErrorCode( BLASTER_DmaError );
            return( BLASTER_Error );
            }
         }

      // Install our interrupt handler
      Irq = BLASTER_Config.Interrupt;
      if ( !VALID_IRQ( Irq ) )
         {
         BLASTER_SetErrorCode( BLASTER_InvalidIrq );
         return( BLASTER_Error );
         }

      Interrupt = BLASTER_Interrupts[ Irq ];
      if ( Interrupt == INVALID )
         {
         BLASTER_SetErrorCode( BLASTER_InvalidIrq );
         return( BLASTER_Error );
         }

      StackSelector = allocateTimerStack( kStackSize );
      if ( StackSelector == NULL )
         {
         BLASTER_SetErrorCode( BLASTER_OutOfMemory );
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
            deallocateTimerStack( StackSelector );
            StackSelector = NULL;
            BLASTER_SetErrorCode( BLASTER_UnableToSetIrq );
            return( BLASTER_Error );
            }
         }

      BLASTER_Installed = TRUE;
      status = BLASTER_Ok;
      }

   BLASTER_SetErrorCode( status );
   return( status );
   }


/*---------------------------------------------------------------------
   Function: BLASTER_Shutdown

   Ends transfer of sound data to the sound card and restores the
   system resources used by the card.
---------------------------------------------------------------------*/

void BLASTER_Shutdown
   (
   void
   )

   {
   int Irq;
   int Interrupt;

   // Halt the DMA transfer
   BLASTER_StopPlayback();

   BLASTER_RestoreVoiceVolume();

   // Reset the DSP
   BLASTER_ResetDSP();

   // Restore the original interrupt
   Irq = BLASTER_Config.Interrupt;
   Interrupt = BLASTER_Interrupts[ Irq ];
   if ( Irq >= 8 )
      {
      IRQ_RestoreVector( Interrupt );
      }
   _dos_setvect( Interrupt, BLASTER_OldInt );

   BLASTER_SoundPlaying = FALSE;

   BLASTER_DMABuffer = NULL;

   BLASTER_SetCallBack( NULL );

   deallocateTimerStack( StackSelector );
   StackSelector = NULL;

   BLASTER_Installed = FALSE;
   }
