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
#include "dma.h"
#include "doomdef.h"

#define VALID   ( 1 == 1 )
#define INVALID ( !VALID )

#define TRUE  ( 1 == 1 )
#define FALSE ( !TRUE )

#define YES ( 1 == 1 )
#define NO  ( !YES )

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
#define MIXER_16BITDMA_INT        0x2
#define MIXER_8BITDMA_INT         0x1
#define MIXER_DisableMPU401Interrupts 0xB
#define MIXER_SBProOutputSetting  0x0E
#define MIXER_SBProStereoFlag     0x02
#define MIXER_SBProVoice          0x04
#define MIXER_SB16VoiceLeft       0x32
#define MIXER_SB16VoiceRight      0x33

#define DSP_Version2xx            0x0200
#define DSP_Version201            0x0201
#define DSP_Version4xx            0x0400

#define DSP_MaxNormalRate         22000

#define DSP_8BitAutoInitMode          0x1c
#define DSP_8BitHighSpeedAutoInitMode 0x90
#define DSP_SetBlockLength            0x48
#define DSP_Old8BitDAC                0x14
#define DSP_16BitDAC                  0xB6
#define DSP_8BitDAC                   0xC6
#define DSP_SetTimeConstant           0x40
#define DSP_Set_DA_Rate               0x41
#define DSP_Set_AD_Rate               0x42
#define DSP_Halt8bitTransfer          0xd0
#define DSP_Halt16bitTransfer         0xd5
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

#define BLASTER_MaxMixMode        STEREO_16BIT

#define MONO_8BIT_SAMPLE_SIZE    1
#define MONO_16BIT_SAMPLE_SIZE   2
#define STEREO_8BIT_SAMPLE_SIZE  ( 2 * MONO_8BIT_SAMPLE_SIZE )
#define STEREO_16BIT_SAMPLE_SIZE ( 2 * MONO_16BIT_SAMPLE_SIZE )

static const int32_t BLASTER_SampleSize[ BLASTER_MaxMixMode + 1 ] =
{
	MONO_8BIT_SAMPLE_SIZE,  STEREO_8BIT_SAMPLE_SIZE,
	MONO_16BIT_SAMPLE_SIZE, STEREO_16BIT_SAMPLE_SIZE
};

typedef struct
{
	int32_t IsSupported;
	int32_t HasMixer;
	int32_t MaxMixMode;
	int32_t MinSamplingRate;
	int32_t MaxSamplingRate;
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


#if defined __DJGPP__
static _go32_dpmi_seginfo BLASTER_OldInt, BLASTER_NewInt;
#elif defined __CCDL__
static uint16_t BLASTER_OldIntSelector;
static uint32_t BLASTER_OldIntOffset;
#elif defined __WATCOMC__
static void (_interrupt _far *BLASTER_OldInt)(void);
#endif


#define UNDEFINED -1

BLASTER_CONFIG BLASTER_Config =
{
	UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED
};

static int32_t BLASTER_Installed = FALSE;
static int32_t BLASTER_Version;

static uint8_t *BLASTER_DMABuffer;
static uint8_t *BLASTER_DMABufferEnd;
static uint8_t *BLASTER_CurrentDMABuffer;
static int32_t  BLASTER_TotalDMABufferSize;

#define BLASTER_DefaultSampleRate 11000
#define BLASTER_DefaultMixMode    MONO_8BIT

static  int32_t BLASTER_TransferLength   = 0;
static  int32_t BLASTER_MixMode          = BLASTER_DefaultMixMode;
static  int32_t BLASTER_SamplePacketSize = MONO_16BIT_SAMPLE_SIZE;
static uint32_t BLASTER_SampleRate       = BLASTER_DefaultSampleRate;

static uint32_t BLASTER_HaltTransferCommand = DSP_Halt8bitTransfer;

static volatile int32_t   BLASTER_SoundPlaying;

static void ( *BLASTER_CallBack )( void );

static int32_t  BLASTER_IntController1Mask;
static int32_t  BLASTER_IntController2Mask;

static int32_t BLASTER_MixerAddress = UNDEFINED;
static int32_t BLASTER_MixerType    = 0;
static int32_t BLASTER_OriginalVoiceVolumeLeft  = 255;
static int32_t BLASTER_OriginalVoiceVolumeRight = 255;

int32_t BLASTER_DMAChannel;

static void BLASTER_DSP1xx_BeginPlayback(int32_t length);

/*---------------------------------------------------------------------
   Function: BLASTER_EnableInterrupt

   Enables the triggering of the sound card interrupt.
---------------------------------------------------------------------*/

static void BLASTER_EnableInterrupt(void)
{
	// Unmask system interrupt
	int32_t mask = inp(0x21) & ~(1 << BLASTER_Config.Interrupt);
	outp(0x21, mask);
}


/*---------------------------------------------------------------------
   Function: BLASTER_DisableInterrupt

   Disables the triggering of the sound card interrupt.
---------------------------------------------------------------------*/

static void BLASTER_DisableInterrupt(void)
{
	int32_t mask;

	// Restore interrupt mask
	mask  = inp(0x21) & ~(1 << BLASTER_Config.Interrupt);
	mask |= BLASTER_IntController1Mask & (1 << BLASTER_Config.Interrupt);
	outp(0x21, mask);
}


/*---------------------------------------------------------------------
   Function: BLASTER_ServiceInterrupt

   Handles interrupt generated by sound card at the end of a voice
   transfer.  Calls the user supplied callback function.
---------------------------------------------------------------------*/

// This is defined because we can't create local variables in a
// function that switches stacks.
static int32_t GlobalStatus;

#if defined __DJGPP__
static void BLASTER_ServiceInterrupt(void)
#elif defined __CCDL__ || defined __WATCOMC__
static void _interrupt _far BLASTER_ServiceInterrupt(void)
#elif defined __DMC__
static int BLASTER_ServiceInterrupt(struct INT_DATA *pd)
#endif
{
	// Acknowledge interrupt
	// Check if this is this an SB16 or newer
	if ( BLASTER_Version >= DSP_Version4xx )
	{
		outp( BLASTER_Config.Address + BLASTER_MixerAddressPort, MIXER_DSP4xxISR_Ack );

		GlobalStatus = inp( BLASTER_Config.Address + BLASTER_MixerDataPort );

		// Check if a 16-bit DMA interrupt occurred
		if ( GlobalStatus & MIXER_16BITDMA_INT )
		{
			// Acknowledge 16-bit transfer interrupt
			inp( BLASTER_Config.Address + BLASTER_16BitDMAAck );
		} else if ( GlobalStatus & MIXER_8BITDMA_INT ) {
			inp( BLASTER_Config.Address + BLASTER_DataAvailablePort );
		} else {
			// Wasn't our interrupt.  Call the old one.
#if defined __DJGPP__
			asm
			(
				"cli \n"
				"pushfl \n"
				"lcall *%0"
				:
				: "m" (BLASTER_OldInt.pm_offset)
			);
#elif defined __WATCOMC__
			_chain_intr(BLASTER_OldInt);
#elif defined __CCDL__
			//TODO call BLASTER_OldInt() instead of acknowledging the interrupt
			outp(0x20, 0x20);
#elif defined __DMC__
			return 0;
#endif
		}
	} else {
		// Older card - can't detect if an interrupt occurred.
		inp( BLASTER_Config.Address + BLASTER_DataAvailablePort );
	}

	// Keep track of current buffer
	BLASTER_CurrentDMABuffer += BLASTER_TransferLength;

	if ( BLASTER_CurrentDMABuffer >= BLASTER_DMABufferEnd )
		BLASTER_CurrentDMABuffer = BLASTER_DMABuffer;

	// Continue playback on cards without autoinit mode
	if ( BLASTER_Version < DSP_Version2xx )
	{
		if ( BLASTER_SoundPlaying )
			BLASTER_DSP1xx_BeginPlayback( BLASTER_TransferLength );
	}

	// Call the caller's callback function
	if ( BLASTER_CallBack != NULL )
		BLASTER_CallBack();

	// send EOI to Interrupt Controller
	outp( 0x20, 0x20 );

#if defined __DMC__
	return 1;
#endif
}


/*---------------------------------------------------------------------
   Function: BLASTER_WriteDSP

   Writes a byte of data to the sound card's DSP.
---------------------------------------------------------------------*/

static void BLASTER_WriteDSP(uint32_t data)
{
	int32_t port = BLASTER_Config.Address + BLASTER_WritePort;

	uint32_t count = 0xFFFF;

	do
	{
		if ((inp(port) & 0x80) == 0)
		{
			outp(port, data);
			break;
		}

		count--;
	} while (count > 0);
}


/*---------------------------------------------------------------------
   Function: BLASTER_ReadDSP

   Reads a byte of data from the sound card's DSP.
---------------------------------------------------------------------*/

static int32_t BLASTER_ReadDSP(void)
{
	int32_t port = BLASTER_Config.Address + BLASTER_DataAvailablePort;

	int32_t status = BLASTER_Error;

	uint32_t count = 0xFFFF;

	do
	{
		if (inp(port) & 0x80)
		{
			status = inp(BLASTER_Config.Address + BLASTER_ReadPort);
			break;
		}

		count--;
	} while( count > 0 );

	return status;
}


/*---------------------------------------------------------------------
   Function: BLASTER_ResetDSP

   Sends a reset command to the sound card's Digital Signal Processor
   (DSP), causing it to perform an initialization.
---------------------------------------------------------------------*/

static int32_t BLASTER_ResetDSP(void)
{
	volatile int32_t count;

	int32_t port = BLASTER_Config.Address + BLASTER_ResetPort;

	int32_t status = BLASTER_CardNotReady;

	outp(port, 1);

	count = 0x100;
	do
	{
		count--;
	} while (count > 0);

	outp(port, 0);

	count = 100;
	do
	{
		if (BLASTER_ReadDSP() == BLASTER_Ready)
		{
			status = BLASTER_Ok;
			break;
		}

		count--;
	} while (count > 0);

	return status;
}


/*---------------------------------------------------------------------
   Function: BLASTER_GetDSPVersion

   Returns the version number of the sound card's DSP.
---------------------------------------------------------------------*/

static int32_t BLASTER_GetDSPVersion(void)
{
	int32_t MajorVersion;
	int32_t MinorVersion;

	BLASTER_WriteDSP(DSP_GetVersion);

	MajorVersion = BLASTER_ReadDSP();
	MinorVersion = BLASTER_ReadDSP();

	if ((MajorVersion == BLASTER_Error) || (MinorVersion == BLASTER_Error))
		return BLASTER_Error;
	else
		return (MajorVersion << 8) + MinorVersion;
}


/*---------------------------------------------------------------------
   Function: BLASTER_SetPlaybackRate

   Sets the rate at which the digitized sound will be played in
   hertz.
---------------------------------------------------------------------*/

#define CalcTimeConstant( rate, samplesize ) \
   ( ( 65536L - ( 256000000L / ( ( samplesize ) * ( rate ) ) ) ) >> 8 )

#define CalcSamplingRate( tc ) \
   ( 256000000L / ( 65536L - ( tc << 8 ) ) )

static void BLASTER_SetPlaybackRate(uint32_t rate)
{
	int32_t LoByte;
	int32_t HiByte;
	CARD_CAPABILITY const *card;

	card = &BLASTER_CardConfig[BLASTER_Config.Type];

	if (BLASTER_Version < DSP_Version4xx)
	{
		int32_t timeconstant;
		int32_t ActualRate;

		// Send sampling rate as time constant for older Sound
		// Blaster compatible cards.

		ActualRate = rate * BLASTER_SamplePacketSize;
		if (ActualRate < card->MinSamplingRate)
			rate = card->MinSamplingRate / BLASTER_SamplePacketSize;

		if (ActualRate > card->MaxSamplingRate)
			rate = card->MaxSamplingRate / BLASTER_SamplePacketSize;

		timeconstant = ( int32_t )CalcTimeConstant( rate, BLASTER_SamplePacketSize );

		// Keep track of what the actual rate is
		BLASTER_SampleRate  = ( uint32_t )CalcSamplingRate( timeconstant );
		BLASTER_SampleRate /= BLASTER_SamplePacketSize;

		BLASTER_WriteDSP( DSP_SetTimeConstant );
		BLASTER_WriteDSP( timeconstant );
	} else {
		// Send literal sampling rate for cards with DSP version
		// 4.xx (Sound Blaster 16)

		BLASTER_SampleRate = rate;

		if ( BLASTER_SampleRate < card->MinSamplingRate )
			BLASTER_SampleRate = card->MinSamplingRate;

		if ( BLASTER_SampleRate > card->MaxSamplingRate )
			BLASTER_SampleRate = card->MaxSamplingRate;

		HiByte = HIBYTE( BLASTER_SampleRate );
		LoByte = LOBYTE( BLASTER_SampleRate );

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

uint32_t BLASTER_GetPlaybackRate(void)
{
	return BLASTER_SampleRate;
}


/*---------------------------------------------------------------------
   Function: BLASTER_SetMixMode

   Sets the sound card to play samples in mono or stereo.
---------------------------------------------------------------------*/

int32_t BLASTER_SetMixMode(int32_t mode)
{
	int32_t CardType = BLASTER_Config.Type;

	mode &= BLASTER_MaxMixMode;

	if ( !( BLASTER_CardConfig[ CardType ].MaxMixMode & STEREO ) )
		mode &= ~STEREO;

	if ( !( BLASTER_CardConfig[ CardType ].MaxMixMode & SIXTEEN_BIT ) )
		mode &= ~SIXTEEN_BIT;

	BLASTER_MixMode = mode;
	BLASTER_SamplePacketSize = BLASTER_SampleSize[ mode ];

	// For the Sound Blaster Pro, we have to set the mixer chip
	// to play mono or stereo samples.

	if ( ( CardType == SBPro ) || ( CardType == SBPro2 ) )
	{
		int32_t data;
		int32_t	port = BLASTER_Config.Address + BLASTER_MixerAddressPort;
		outp( port, MIXER_SBProOutputSetting );

		port = BLASTER_Config.Address + BLASTER_MixerDataPort;

		// Get current mode
		data = inp( port );

		// set stereo mode bit
		if ( mode & STEREO )
			data |= MIXER_SBProStereoFlag;
		else
			data &= ~MIXER_SBProStereoFlag;

		// set the mode
		outp( port, data );

		BLASTER_SetPlaybackRate( BLASTER_SampleRate );
	}

	return mode;
}


/*---------------------------------------------------------------------
   Function: BLASTER_StopPlayback

   Ends the DMA transfer of digitized sound to the sound card.
---------------------------------------------------------------------*/

void BLASTER_StopPlayback(void)
{
	int32_t DmaChannel;

	// Don't allow anymore interrupts
	BLASTER_DisableInterrupt();

	if ( BLASTER_HaltTransferCommand == DSP_Reset )
		BLASTER_ResetDSP();
	else
		BLASTER_WriteDSP( BLASTER_HaltTransferCommand );

	// Disable the DMA channel
	DmaChannel = BLASTER_MixMode & SIXTEEN_BIT ? BLASTER_Config.Dma16 : BLASTER_Config.Dma8;
	DMA_EndTransfer( DmaChannel );

	// Turn off speaker
	BLASTER_WriteDSP(DSP_SpeakerOff);

	BLASTER_SoundPlaying = FALSE;

	BLASTER_DMABuffer = NULL;
}


/*---------------------------------------------------------------------
   Function: BLASTER_SetupDMABuffer

   Programs the DMAC for sound transfer.
---------------------------------------------------------------------*/

static int32_t BLASTER_SetupDMABuffer(uint8_t *BufferPtr, int32_t BufferSize)
{
	int32_t DmaStatus;

	int32_t DmaChannel = BLASTER_MixMode & SIXTEEN_BIT ? BLASTER_Config.Dma16 : BLASTER_Config.Dma8;
	if ( DmaChannel == UNDEFINED )
		return BLASTER_Error;

	DmaStatus = DMA_SetupTransfer( DmaChannel, BufferPtr, BufferSize );
	if ( DmaStatus == DMA_Error )
		return BLASTER_Error;

	BLASTER_DMAChannel = DmaChannel;

	BLASTER_DMABuffer          = BufferPtr;
	BLASTER_CurrentDMABuffer   = BufferPtr;
	BLASTER_TotalDMABufferSize = BufferSize;
	BLASTER_DMABufferEnd       = BufferPtr + BufferSize;

	return BLASTER_Ok;
}


/*---------------------------------------------------------------------
   Function: BLASTER_DSP1xx_BeginPlayback

   Starts playback of digitized sound on cards compatible with DSP
   version 1.xx.
---------------------------------------------------------------------*/

static void BLASTER_DSP1xx_BeginPlayback(int32_t length)
{
	int32_t SampleLength = length - 1;
	int32_t HiByte = HIBYTE( SampleLength );
	int32_t LoByte = LOBYTE( SampleLength );

	// Program DSP to play sound
	BLASTER_WriteDSP( DSP_Old8BitDAC );
	BLASTER_WriteDSP( LoByte );
	BLASTER_WriteDSP( HiByte );

	BLASTER_HaltTransferCommand = DSP_Halt8bitTransfer;

	BLASTER_SoundPlaying = TRUE;
}


/*---------------------------------------------------------------------
   Function: BLASTER_DSP2xx_BeginPlayback

   Starts playback of digitized sound on cards compatible with DSP
   version 2.xx.
---------------------------------------------------------------------*/

static void BLASTER_DSP2xx_BeginPlayback(int32_t length)
{
	int32_t SampleLength = length - 1;
	int32_t HiByte = HIBYTE( SampleLength );
	int32_t LoByte = LOBYTE( SampleLength );

	BLASTER_WriteDSP( DSP_SetBlockLength );
	BLASTER_WriteDSP( LoByte );
	BLASTER_WriteDSP( HiByte );

	if ( ( BLASTER_Version >= DSP_Version201 ) && ( DSP_MaxNormalRate < ( BLASTER_SampleRate * BLASTER_SamplePacketSize ) ) )
	{
		BLASTER_WriteDSP( DSP_8BitHighSpeedAutoInitMode );
		BLASTER_HaltTransferCommand = DSP_Reset;
	} else {
		BLASTER_WriteDSP( DSP_8BitAutoInitMode );
		BLASTER_HaltTransferCommand = DSP_Halt8bitTransfer;
	}

	BLASTER_SoundPlaying = TRUE;
}


/*---------------------------------------------------------------------
   Function: BLASTER_DSP4xx_BeginPlayback

   Starts playback of digitized sound on cards compatible with DSP
   version 4.xx, such as the Sound Blaster 16.
---------------------------------------------------------------------*/

static void BLASTER_DSP4xx_BeginPlayback(int32_t length)
{
	int32_t TransferCommand;
	int32_t TransferMode;
	int32_t SampleLength;
	int32_t LoByte;
	int32_t HiByte;

	if ( BLASTER_MixMode & SIXTEEN_BIT )
	{
		TransferCommand = DSP_16BitDAC;
		SampleLength = ( length / 2 ) - 1;
		BLASTER_HaltTransferCommand = DSP_Halt16bitTransfer;
		if ( BLASTER_MixMode & STEREO )
			TransferMode = DSP_SignedStereoData;
		else
			TransferMode = DSP_SignedMonoData;
	} else {
		TransferCommand = DSP_8BitDAC;
		SampleLength = length - 1;
		BLASTER_HaltTransferCommand = DSP_Halt8bitTransfer;
		if ( BLASTER_MixMode & STEREO )
			TransferMode = DSP_UnsignedStereoData;
		else
			TransferMode = DSP_UnsignedMonoData;
	}

	HiByte = HIBYTE( SampleLength );
	LoByte = LOBYTE( SampleLength );

	// Program DSP to play sound
	BLASTER_WriteDSP( TransferCommand );
	BLASTER_WriteDSP( TransferMode );
	BLASTER_WriteDSP( LoByte );
	BLASTER_WriteDSP( HiByte );

	BLASTER_SoundPlaying = TRUE;
}


/*---------------------------------------------------------------------
   Function: BLASTER_BeginBufferedPlayback

   Begins multibuffered playback of digitized sound on the sound card.
---------------------------------------------------------------------*/

int32_t BLASTER_BeginBufferedPlayback(uint8_t *BufferStart, int32_t BufferSize, int32_t NumDivisions, uint32_t SampleRate, int32_t MixMode, void (*CallBackFunc)(void))
{
	int32_t DmaStatus;
	int32_t TransferLength;

	if ( BLASTER_SoundPlaying )
		BLASTER_StopPlayback();

	BLASTER_SetMixMode( MixMode );

	DmaStatus = BLASTER_SetupDMABuffer( BufferStart, BufferSize );
	if ( DmaStatus == BLASTER_Error )
		return BLASTER_Error;

	BLASTER_SetPlaybackRate( SampleRate );

	// Specifie the user function to call at the end of a sound transfer
	BLASTER_CallBack = CallBackFunc;

	BLASTER_EnableInterrupt();

	// Turn on speaker
	BLASTER_WriteDSP(DSP_SpeakerOn);

	TransferLength = BufferSize / NumDivisions;
	BLASTER_TransferLength = TransferLength;

	//  Program the sound card to start the transfer.
	if ( BLASTER_Version < DSP_Version2xx )
		BLASTER_DSP1xx_BeginPlayback( TransferLength );
	else if ( BLASTER_Version < DSP_Version4xx )
		BLASTER_DSP2xx_BeginPlayback( TransferLength );
	else
		BLASTER_DSP4xx_BeginPlayback( TransferLength );

	return BLASTER_Ok;
}


/*---------------------------------------------------------------------
   Function: BLASTER_WriteMixer

   Writes a byte of data to the Sound Blaster's mixer chip.
---------------------------------------------------------------------*/

static void BLASTER_WriteMixer(int32_t reg, int32_t data)
{
	outp(BLASTER_MixerAddress + BLASTER_MixerAddressPort, reg);
	outp(BLASTER_MixerAddress + BLASTER_MixerDataPort,   data);
}


/*---------------------------------------------------------------------
   Function: BLASTER_ReadMixer

   Reads a byte of data from the Sound Blaster's mixer chip.
---------------------------------------------------------------------*/

static int32_t BLASTER_ReadMixer(int32_t reg)
{
	outp(BLASTER_MixerAddress + BLASTER_MixerAddressPort, reg);
	return inp(BLASTER_MixerAddress + BLASTER_MixerDataPort);
}


/*---------------------------------------------------------------------
   Function: BLASTER_SetVoiceVolume

   Sets the volume of the digitized sound channel on the Sound
   Blaster's mixer chip.
---------------------------------------------------------------------*/

#define volume 255

void BLASTER_SetVoiceVolume(void)
{
	int32_t data;

	switch (BLASTER_MixerType)
	{
		case SBPro:
		case SBPro2:
			data = (volume & 0xf0) + (volume >> 4);
			BLASTER_WriteMixer(MIXER_SBProVoice, data);
			break;

		case SB16:
			BLASTER_WriteMixer(MIXER_SB16VoiceLeft,  volume & 0xf8);
			BLASTER_WriteMixer(MIXER_SB16VoiceRight, volume & 0xf8);
			break;
	}
}


/*---------------------------------------------------------------------
   Function: BLASTER_CardHasMixer

   Checks if the selected Sound Blaster card has a mixer.
---------------------------------------------------------------------*/

int32_t BLASTER_CardHasMixer(void)
{
	return BLASTER_CardConfig[BLASTER_MixerType].HasMixer;
}


/*---------------------------------------------------------------------
   Function: BLASTER_SaveVoiceVolume

   Saves the user's voice mixer settings.
---------------------------------------------------------------------*/

static void BLASTER_SaveVoiceVolume(void)
{
	if ( BLASTER_CardHasMixer() )
	{
		switch( BLASTER_MixerType )
		{
			case SBPro :
			case SBPro2 :
				BLASTER_OriginalVoiceVolumeLeft  = BLASTER_ReadMixer( MIXER_SBProVoice );
				break;

			case SB16 :
				BLASTER_OriginalVoiceVolumeLeft  = BLASTER_ReadMixer( MIXER_SB16VoiceLeft );
				BLASTER_OriginalVoiceVolumeRight = BLASTER_ReadMixer( MIXER_SB16VoiceRight );
				break;
		}
	}
}


/*---------------------------------------------------------------------
   Function: BLASTER_RestoreVoiceVolume

   Restores the user's voice mixer settings.
---------------------------------------------------------------------*/

static void BLASTER_RestoreVoiceVolume(void)
{
	if ( BLASTER_CardHasMixer() )
	{
		switch( BLASTER_MixerType )
		{
			case SBPro :
			case SBPro2 :
				BLASTER_WriteMixer( MIXER_SBProVoice, BLASTER_OriginalVoiceVolumeLeft );
				break;

			case SB16 :
				BLASTER_WriteMixer( MIXER_SB16VoiceLeft,  BLASTER_OriginalVoiceVolumeLeft );
				BLASTER_WriteMixer( MIXER_SB16VoiceRight, BLASTER_OriginalVoiceVolumeRight );
				break;
		}
	}
}


/*---------------------------------------------------------------------
   Function: BLASTER_GetEnv

   Retrieves the BLASTER environment settings and returns them to
   the caller.
---------------------------------------------------------------------*/

#define BlasterEnv_Address    'A'
#define BlasterEnv_Interrupt  'I'
#define BlasterEnv_8bitDma    'D'
#define BlasterEnv_16bitDma   'H'
#define BlasterEnv_Type       'T'

int32_t BLASTER_GetEnv(BLASTER_CONFIG *Config)
{
	char *Blaster;
	char parameter;
	int32_t  status;
	int32_t  errorcode;

	Config->Address   = UNDEFINED;
	Config->Type      = UNDEFINED;
	Config->Interrupt = UNDEFINED;
	Config->Dma8      = UNDEFINED;
	Config->Dma16     = UNDEFINED;

	Blaster = getenv( "BLASTER" );
	if ( Blaster == NULL )
		return BLASTER_Error;

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
			return BLASTER_Error;

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
			default  :
				// Skip the offending data
				// sscanf( Blaster, "%*s" );
				break;
		}

		while( isxdigit( *Blaster ) )
			Blaster++;
	}

	status    = BLASTER_Ok;
	errorcode = BLASTER_Ok;

	if ( Config->Type == UNDEFINED )
		errorcode = BLASTER_CardTypeNotSet;
	else if ( ( Config->Type < BLASTER_MinCardType ) || ( Config->Type > BLASTER_MaxCardType ) || ( !BLASTER_CardConfig[ Config->Type ].IsSupported ) )
		errorcode = BLASTER_UnsupportedCardType;

	if ( Config->Dma8 == UNDEFINED )
		errorcode = BLASTER_DMANotSet;

	if ( Config->Interrupt == UNDEFINED )
		errorcode = BLASTER_IntNotSet;

	if ( Config->Address == UNDEFINED )
		errorcode = BLASTER_AddrNotSet;

	if ( errorcode != BLASTER_Ok )
		status = BLASTER_Error;

	return status;
}


/*---------------------------------------------------------------------
   Function: BLASTER_SetCardSettings

   Sets up the sound card's parameters.
---------------------------------------------------------------------*/

void BLASTER_SetCardSettings(BLASTER_CONFIG Config)
{
	if ( BLASTER_Installed )
		BLASTER_Shutdown();

	if ( ( Config.Type < BLASTER_MinCardType ) || ( Config.Type > BLASTER_MaxCardType ) || ( !BLASTER_CardConfig[ Config.Type ].IsSupported ) )
		return;

	BLASTER_Config.Address   = Config.Address;
	BLASTER_Config.Type      = Config.Type;
	BLASTER_Config.Interrupt = Config.Interrupt;
	BLASTER_Config.Dma8      = Config.Dma8;
	BLASTER_Config.Dma16     = Config.Dma16;

	BLASTER_MixerAddress     = Config.Address;
	BLASTER_MixerType        = Config.Type;
}


/*---------------------------------------------------------------------
   Function: BLASTER_SetupWaveBlaster

   Allows the WaveBlaster to play music while the Sound Blaster 16
   plays digital sound.
---------------------------------------------------------------------*/

void BLASTER_SetupWaveBlaster(void)
{
	if (BLASTER_MixerType == SB16)
	{
		// Disable MPU401 interrupts.  If they are not disabled,
		// the SB16 will not produce sound or music.
		BLASTER_WriteMixer(MIXER_DSP4xxISR_Enable, MIXER_DisableMPU401Interrupts);
	}
}


/*---------------------------------------------------------------------
   Function: BLASTER_Init

   Initializes the sound card and prepares the module to play
   digitized sounds.
---------------------------------------------------------------------*/

int32_t BLASTER_Init(void)
{
	int32_t status;

	if ( BLASTER_Installed )
		BLASTER_Shutdown();

	// Save the interrupt masks
	BLASTER_IntController1Mask = inp( 0x21 );
	BLASTER_IntController2Mask = inp( 0xA1 );

	status = BLASTER_ResetDSP();
	if ( status == BLASTER_Ok )
	{
		BLASTER_SaveVoiceVolume();

		BLASTER_SoundPlaying = FALSE;

		BLASTER_CallBack = NULL;

		BLASTER_DMABuffer = NULL;

		BLASTER_Version = BLASTER_GetDSPVersion();

		BLASTER_SetPlaybackRate( BLASTER_DefaultSampleRate );
		BLASTER_SetMixMode( BLASTER_DefaultMixMode );

		if ( BLASTER_Config.Dma16 != UNDEFINED )
		{
			status = DMA_VerifyChannel( BLASTER_Config.Dma16 );
			if ( status == DMA_Error )
				return BLASTER_Error;
		}

		if ( BLASTER_Config.Dma8 != UNDEFINED )
		{
			status = DMA_VerifyChannel( BLASTER_Config.Dma8 );
			if ( status == DMA_Error )
				return BLASTER_Error;
		}

		// Install our interrupt handler
		if (!(BLASTER_Config.Interrupt == 2 || BLASTER_Config.Interrupt == 5 || BLASTER_Config.Interrupt == 7))
			return BLASTER_Error;

#if defined __DJGPP__
		_go32_dpmi_get_protected_mode_interrupt_vector(BLASTER_Config.Interrupt + 8, &BLASTER_OldInt);

		BLASTER_NewInt.pm_selector = _go32_my_cs(); 
		BLASTER_NewInt.pm_offset = (int32_t)BLASTER_ServiceInterrupt;
		_go32_dpmi_allocate_iret_wrapper(&BLASTER_NewInt);
		_go32_dpmi_set_protected_mode_interrupt_vector(BLASTER_Config.Interrupt + 8, &BLASTER_NewInt);
#elif defined __DMC__
		int_intercept(BLASTER_Config.Interrupt + 8, BLASTER_ServiceInterrupt, 0);
#elif defined __CCDL__
		{
			struct SREGS	segregs;

			_segread(&segregs);
			dpmi_get_protected_interrupt(&BLASTER_OldIntSelector, &BLASTER_OldIntOffset, BLASTER_Config.Interrupt + 8);
			dpmi_set_protected_interrupt(BLASTER_Config.Interrupt + 8, segregs.cs, (uint32_t)BLASTER_ServiceInterrupt);
		}
#elif defined __WATCOMC__
		BLASTER_OldInt = _dos_getvect(BLASTER_Config.Interrupt + 8);
		_dos_setvect(BLASTER_Config.Interrupt + 8, BLASTER_ServiceInterrupt);
#endif

		BLASTER_Installed = TRUE;
		status = BLASTER_Ok;
	}

	return status;
}


/*---------------------------------------------------------------------
   Function: BLASTER_Shutdown

   Ends transfer of sound data to the sound card and restores the
   system resources used by the card.
---------------------------------------------------------------------*/

void BLASTER_Shutdown(void)
{
	// Halt the DMA transfer
	BLASTER_StopPlayback();

	BLASTER_RestoreVoiceVolume();

	// Reset the DSP
	BLASTER_ResetDSP();

	// Restore the original interrupt
#if defined __DJGPP__
	_go32_dpmi_set_protected_mode_interrupt_vector(BLASTER_Config.Interrupt + 8, &BLASTER_OldInt);
	_go32_dpmi_free_iret_wrapper(&BLASTER_NewInt);
#elif defined __DMC__
	int_restore(BLASTER_Config.Interrupt + 8);
#elif defined __CCDL__
	dpmi_set_protected_interrupt(BLASTER_Config.Interrupt + 8, BLASTER_OldIntSelector, BLASTER_OldIntOffset);
#elif defined __WATCOMC__
	_dos_setvect(BLASTER_Config.Interrupt + 8, BLASTER_OldInt);
#endif

	BLASTER_SoundPlaying = FALSE;

	BLASTER_DMABuffer = NULL;

	BLASTER_CallBack = NULL;

	BLASTER_Installed = FALSE;
}
