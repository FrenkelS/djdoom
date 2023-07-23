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

#include <dos.h>
#include <time.h>
#include "id_heads.h"
#include "dmx.h"
#include "a_blast.h"
#include "a_dma.h"
#include "a_inter.h"
#include "a_ll_man.h"
#include "a_multiv.h"

enum MV_Errors
{
	MV_Error   = -1,
	MV_Ok      = 0,
	MV_UnsupportedCard,
	MV_NotInstalled,
	MV_NoVoices,
	MV_NoMem,
	MV_VoiceNotFound,
	MV_BlasterError,
	MV_DMAFailure
};

typedef enum
{
	NoMoreData,
	KeepPlaying
} playbackstatus;


typedef struct VoiceNode
{
	struct VoiceNode *next;
	struct VoiceNode *prev;

	void ( *mix )( void );

	uint8_t *NextBlock;
	uint32_t BlockLength;

	uint32_t FixedPointBufferSize;

	uint8_t *sound;
	uint32_t length;
	uint32_t SamplingRate;
	uint32_t RateScale;
	uint32_t position;
	boolean  Playing;

	int32_t  handle;
	int32_t  priority;

	int16_t *LeftVolume;
	int16_t *RightVolume;
} VoiceNode;

static playbackstatus MV_GetNextRawBlock(VoiceNode *voice);

typedef struct
{
	VoiceNode *start;
	VoiceNode *end;
} VList;

#define MV_NumVoices       8

typedef uint8_t HARSH_CLIP_TABLE_8[ MV_NumVoices * 256 ];

void MV_Mix8BitMono(void);
void MV_Mix8BitStereo(void);
void MV_Mix16BitMono(void);
void MV_Mix16BitStereo(void);


#define IS_QUIET( ptr )  ( ( void * )( ptr ) == ( void * )&MV_VolumeTable[ 0 ] )

#define MV_MaxVolume       63

//static signed short MV_VolumeTable[ MV_MaxVolume + 1 ][ 256 ];
static int16_t MV_VolumeTable[ 63 + 1 ][ 256 ];

static boolean MV_Installed   = false;
static int32_t MV_SoundCard   = AHW_SOUND_BLASTER;
static int32_t MV_MaxVoices   = 1;


#define MixBufferSize     256

#define NumberOfBuffers   16
#define TotalBufferSize   (MixBufferSize * NumberOfBuffers)

static int32_t MV_BufferSize      = MixBufferSize;
static int32_t MV_NumberOfBuffers = NumberOfBuffers;

static int32_t MV_MixMode    = MONO_8BIT;
static int32_t MV_Channels   = 1;
static int32_t MV_Bits       = 8;

#define SILENCE_16BIT     0
#define SILENCE_8BIT      0x80808080

static int32_t MV_Silence    = SILENCE_8BIT;
static boolean MV_SwapLeftRight = false;

static int32_t MV_RequestedMixRate;
static int32_t MV_MixRate;

static int32_t MV_DMAChannel = -1;
static int32_t MV_BuffShift;

static int32_t MV_TotalMemory;

static uint16_t MV_BufferDescriptor;
static boolean  MV_BufferEmpty[NumberOfBuffers];
static uint8_t *MV_MixBuffer[NumberOfBuffers + 1];

static VoiceNode *MV_Voices = NULL;

static volatile VList VoiceList = { NULL, NULL };
static volatile VList VoicePool = { NULL, NULL };

#define MV_MinVoiceHandle  1

static int32_t MV_MixPage      = 0;
static int32_t MV_VoiceHandle  = MV_MinVoiceHandle;

uint8_t *MV_HarshClipTable		__attribute__ ((externally_visible));
uint8_t *MV_MixDestination		__attribute__ ((externally_visible));
uint32_t MV_MixPosition			__attribute__ ((externally_visible));
int16_t *MV_LeftVolume			__attribute__ ((externally_visible));
int16_t *MV_RightVolume			__attribute__ ((externally_visible));
int32_t  MV_SampleSize			__attribute__ ((externally_visible)) = 1;
int32_t  MV_RightChannelOffset	__attribute__ ((externally_visible));


static int32_t MV_ErrorCode = MV_Ok;

#define MV_SetErrorCode( status ) \
   MV_ErrorCode   = ( status )


/*---------------------------------------------------------------------
   Function: MV_Mix

   Mixes the sound into the buffer.
---------------------------------------------------------------------*/

uint32_t MV_Position	__attribute__ ((externally_visible));
uint32_t MV_Rate		__attribute__ ((externally_visible));
uint8_t *MV_Start		__attribute__ ((externally_visible));
uint32_t MV_Length		__attribute__ ((externally_visible));

static void MV_Mix(VoiceNode *voice, int32_t buffer)
{
	int32_t   length;
	uint32_t  FixedPointBufferSize;

	if ((voice->length == 0) && (MV_GetNextRawBlock(voice) != KeepPlaying))
		return;

	length               = MixBufferSize;
	FixedPointBufferSize = voice->FixedPointBufferSize;

	MV_MixDestination    = MV_MixBuffer[buffer];
	MV_LeftVolume        = voice->LeftVolume;
	MV_RightVolume       = voice->RightVolume;

	if ((MV_Channels == 2) && (IS_QUIET(MV_LeftVolume)))
	{
		MV_LeftVolume      = MV_RightVolume;
		MV_MixDestination += MV_RightChannelOffset;
	}

	// Add this voice to the mix
	while (length > 0)
	{
		MV_Start    = voice->sound;
		MV_Rate     = voice->RateScale;
		MV_Position = voice->position;

		// Check if the last sample in this buffer would be
		// beyond the length of the sample block
		if ((MV_Position + FixedPointBufferSize) >= voice->length)
		{
			if (MV_Position < voice->length)
				MV_Length = (voice->length - MV_Position + MV_Rate - 1) / MV_Rate;
			else
			{
				MV_GetNextRawBlock(voice);
				return;
			}
		}
		else
			MV_Length = length;

		voice->mix();

		if (MV_Length & 1)
		{
			MV_MixPosition += MV_Rate;
			MV_Length      -= 1;
		}
		voice->position = MV_MixPosition;

		length -= MV_Length;

		if (voice->position >= voice->length)
		{
			// Get the next block of sound
			if (MV_GetNextRawBlock(voice) != KeepPlaying)
				return;

			if (length > 0)
			{
				// Get the position of the last sample in the buffer
				FixedPointBufferSize = voice->RateScale * (length - 1);
			}
		}
	}
}


/*---------------------------------------------------------------------
   Function: MV_PlayVoice

   Adds a voice to the play list.
---------------------------------------------------------------------*/

static void MV_PlayVoice(VoiceNode *voice)
{
	uint32_t flags = DisableInterrupts();

	LL_AddToTail(VoiceNode, &VoiceList, voice);

	RestoreInterrupts(flags);
}


/*---------------------------------------------------------------------
   Function: MV_StopVoice

   Removes the voice from the play list and adds it to the free list.
---------------------------------------------------------------------*/

static void MV_StopVoice(VoiceNode *voice)
{
	uint32_t flags = DisableInterrupts();

	// move the voice from the play list to the free list
	LL_Remove(VoiceNode, &VoiceList, voice);
	LL_AddToTail(VoiceNode, &VoicePool, voice);

	RestoreInterrupts(flags);
}


static void ClearBuffer_DW(void *ptr, uint32_t data, int32_t length)
{
	int32_t i;

	for (i = 0; i < length; i++)
		((uint32_t *)ptr)[i] = data;
}


/*---------------------------------------------------------------------
   Function: MV_ServiceVoc

   Starts playback of the waiting buffer and mixes the next one.
---------------------------------------------------------------------*/

static void MV_ServiceVoc(void)
{
	VoiceNode *voice;
	VoiceNode *next;
	uint8_t   *buffer;

	if (MV_DMAChannel >= 0)
	{
		// Get the currently playing buffer
		buffer = DMA_GetCurrentPos(MV_DMAChannel);
		MV_MixPage   = (uint32_t)(buffer - MV_MixBuffer[0]);
		MV_MixPage >>= MV_BuffShift;
	}

	// Toggle which buffer we'll mix next
	MV_MixPage++;

	if (MV_MixPage >= MV_NumberOfBuffers)
		MV_MixPage -= MV_NumberOfBuffers;

	// Initialize buffer
	//Commented out so that the buffer is always cleared.
	//This is so the guys at Echo Speech can mix into the
	//buffer even when no sounds are playing.
	if (!MV_BufferEmpty[MV_MixPage])
	{
		ClearBuffer_DW(MV_MixBuffer[MV_MixPage], MV_Silence, MV_BufferSize >> 2);
		MV_BufferEmpty[MV_MixPage] = true;
	}

	// Play any waiting voices
	voice = VoiceList.start;
	while (voice != NULL)
	{
		MV_BufferEmpty[MV_MixPage] = false;

		MV_Mix(voice, MV_MixPage);

		next = voice->next;

		// Is this voice done?
		if (!voice->Playing)
			MV_StopVoice(voice);

		voice = next;
	}
}


/*---------------------------------------------------------------------
   Function: MV_GetNextRawBlock

   Controls playback of demand fed data.
---------------------------------------------------------------------*/

static playbackstatus MV_GetNextRawBlock(VoiceNode *voice)
{
	if (voice->BlockLength == 0)
	{
		voice->Playing = false;

		return NoMoreData;
	} else {
		voice->sound        = voice->NextBlock;
		voice->position    -= voice->length;
		voice->length       = voice->BlockLength > 0x8000 ? 0x8000 : voice->BlockLength;
		voice->NextBlock   += voice->length;
		voice->BlockLength -= voice->length;
		voice->length     <<= 16;

		return KeepPlaying;
	}
}


/*---------------------------------------------------------------------
   Function: MV_GetVoice

   Locates the voice with the specified handle.
---------------------------------------------------------------------*/

static VoiceNode *MV_GetVoice(int32_t handle)
{
	VoiceNode *voice;
	
	uint32_t flags = DisableInterrupts();

	voice = VoiceList.start;
	while (voice != NULL)
	{
		if (handle == voice->handle)
			break;

		voice = voice->next;
	}

	RestoreInterrupts(flags);

	if (voice == NULL)
		MV_SetErrorCode(MV_VoiceNotFound);

	return voice;
}


/*---------------------------------------------------------------------
   Function: MV_VoicePlaying

   Checks if the voice associated with the specified handle is
   playing.
---------------------------------------------------------------------*/

boolean MV_VoicePlaying(int32_t handle)
{
	VoiceNode *voice;

	if (!MV_Installed)
	{
		MV_SetErrorCode(MV_NotInstalled);
		return false;
	}

	voice = MV_GetVoice(handle);

	return voice != NULL;
}


/*---------------------------------------------------------------------
   Function: MV_KillAllVoices

   Stops output of all currently active voices.
---------------------------------------------------------------------*/

static void MV_KillAllVoices(void)
{
	if (!MV_Installed)
	{
		MV_SetErrorCode(MV_NotInstalled);
		return;
	}

	// Remove all the voices from the list
	while (VoiceList.start != NULL)
	{
		MV_Kill(VoiceList.start->handle);
	}
}


/*---------------------------------------------------------------------
   Function: MV_Kill

   Stops output of the voice associated with the specified handle.
---------------------------------------------------------------------*/

void MV_Kill(int32_t handle)
{
	VoiceNode *voice;
	uint32_t  flags;

	if (!MV_Installed)
	{
		MV_SetErrorCode(MV_NotInstalled);
		return;
	}

	flags = DisableInterrupts();

	voice = MV_GetVoice(handle);
	if (voice == NULL)
		MV_SetErrorCode(MV_VoiceNotFound);
	else
		MV_StopVoice(voice);

	RestoreInterrupts(flags);
}


/*---------------------------------------------------------------------
   Function: MV_AllocVoice

   Retrieve an inactive or lower priority voice for output.
---------------------------------------------------------------------*/

static VoiceNode *MV_AllocVoice(int32_t priority)
{
	VoiceNode   *voice;
	VoiceNode   *node;

	uint32_t flags = DisableInterrupts();

	// Check if we have any free voices
	if (LL_Empty(&VoicePool))
	{
		// check if we have a higher priority than a voice that is playing.
		voice = node = VoiceList.start;
		while (node != NULL)
		{
			if (node->priority < voice->priority)
				voice = node;

			node = node->next;
		}

		if (priority >= voice->priority)
			MV_Kill( voice->handle );
	}

	// Check if any voices are in the voice pool
	if (LL_Empty(&VoicePool))
	{
		// No free voices
		RestoreInterrupts(flags);
		return NULL;
	}

	voice = VoicePool.start;
	LL_Remove(VoiceNode, &VoicePool, voice);
	RestoreInterrupts(flags);

	// Find a free voice handle
	do
	{
		MV_VoiceHandle++;
		if (MV_VoiceHandle < MV_MinVoiceHandle)
			MV_VoiceHandle = MV_MinVoiceHandle;
	} while (MV_VoicePlaying(MV_VoiceHandle));

	voice->handle = MV_VoiceHandle;

	return voice;
}


/*---------------------------------------------------------------------
   Function: PITCH_GetScale

   Returns a fixed-point value to scale number the specified amount.
---------------------------------------------------------------------*/

#define MAXDETUNE 25

static const uint32_t PitchTable[ 12 ][ MAXDETUNE ] =
   {
      { 0x10000, 0x10097, 0x1012f, 0x101c7, 0x10260, 0x102f9, 0x10392, 0x1042c,
      0x104c6, 0x10561, 0x105fb, 0x10696, 0x10732, 0x107ce, 0x1086a, 0x10907,
      0x109a4, 0x10a41, 0x10adf, 0x10b7d, 0x10c1b, 0x10cba, 0x10d59, 0x10df8,
      0x10e98 },
      { 0x10f38, 0x10fd9, 0x1107a, 0x1111b, 0x111bd, 0x1125f, 0x11302, 0x113a5,
      0x11448, 0x114eb, 0x1158f, 0x11634, 0x116d8, 0x1177e, 0x11823, 0x118c9,
      0x1196f, 0x11a16, 0x11abd, 0x11b64, 0x11c0c, 0x11cb4, 0x11d5d, 0x11e06,
      0x11eaf },
      { 0x11f59, 0x12003, 0x120ae, 0x12159, 0x12204, 0x122b0, 0x1235c, 0x12409,
      0x124b6, 0x12563, 0x12611, 0x126bf, 0x1276d, 0x1281c, 0x128cc, 0x1297b,
      0x12a2b, 0x12adc, 0x12b8d, 0x12c3e, 0x12cf0, 0x12da2, 0x12e55, 0x12f08,
      0x12fbc },
      { 0x1306f, 0x13124, 0x131d8, 0x1328d, 0x13343, 0x133f9, 0x134af, 0x13566,
      0x1361d, 0x136d5, 0x1378d, 0x13846, 0x138fe, 0x139b8, 0x13a72, 0x13b2c,
      0x13be6, 0x13ca1, 0x13d5d, 0x13e19, 0x13ed5, 0x13f92, 0x1404f, 0x1410d,
      0x141cb },
      { 0x1428a, 0x14349, 0x14408, 0x144c8, 0x14588, 0x14649, 0x1470a, 0x147cc,
      0x1488e, 0x14951, 0x14a14, 0x14ad7, 0x14b9b, 0x14c5f, 0x14d24, 0x14dea,
      0x14eaf, 0x14f75, 0x1503c, 0x15103, 0x151cb, 0x15293, 0x1535b, 0x15424,
      0x154ee },
      { 0x155b8, 0x15682, 0x1574d, 0x15818, 0x158e4, 0x159b0, 0x15a7d, 0x15b4a,
      0x15c18, 0x15ce6, 0x15db4, 0x15e83, 0x15f53, 0x16023, 0x160f4, 0x161c5,
      0x16296, 0x16368, 0x1643a, 0x1650d, 0x165e1, 0x166b5, 0x16789, 0x1685e,
      0x16934 },
      { 0x16a09, 0x16ae0, 0x16bb7, 0x16c8e, 0x16d66, 0x16e3e, 0x16f17, 0x16ff1,
      0x170ca, 0x171a5, 0x17280, 0x1735b, 0x17437, 0x17513, 0x175f0, 0x176ce,
      0x177ac, 0x1788a, 0x17969, 0x17a49, 0x17b29, 0x17c09, 0x17cea, 0x17dcc,
      0x17eae },
      { 0x17f91, 0x18074, 0x18157, 0x1823c, 0x18320, 0x18406, 0x184eb, 0x185d2,
      0x186b8, 0x187a0, 0x18888, 0x18970, 0x18a59, 0x18b43, 0x18c2d, 0x18d17,
      0x18e02, 0x18eee, 0x18fda, 0x190c7, 0x191b5, 0x192a2, 0x19391, 0x19480,
      0x1956f },
      { 0x1965f, 0x19750, 0x19841, 0x19933, 0x19a25, 0x19b18, 0x19c0c, 0x19d00,
      0x19df4, 0x19ee9, 0x19fdf, 0x1a0d5, 0x1a1cc, 0x1a2c4, 0x1a3bc, 0x1a4b4,
      0x1a5ad, 0x1a6a7, 0x1a7a1, 0x1a89c, 0x1a998, 0x1aa94, 0x1ab90, 0x1ac8d,
      0x1ad8b },
      { 0x1ae89, 0x1af88, 0x1b088, 0x1b188, 0x1b289, 0x1b38a, 0x1b48c, 0x1b58f,
      0x1b692, 0x1b795, 0x1b89a, 0x1b99f, 0x1baa4, 0x1bbaa, 0x1bcb1, 0x1bdb8,
      0x1bec0, 0x1bfc9, 0x1c0d2, 0x1c1dc, 0x1c2e6, 0x1c3f1, 0x1c4fd, 0x1c609,
      0x1c716 },
      { 0x1c823, 0x1c931, 0x1ca40, 0x1cb50, 0x1cc60, 0x1cd70, 0x1ce81, 0x1cf93,
      0x1d0a6, 0x1d1b9, 0x1d2cd, 0x1d3e1, 0x1d4f6, 0x1d60c, 0x1d722, 0x1d839,
      0x1d951, 0x1da69, 0x1db82, 0x1dc9c, 0x1ddb6, 0x1ded1, 0x1dfec, 0x1e109,
      0x1e225 },
      { 0x1e343, 0x1e461, 0x1e580, 0x1e6a0, 0x1e7c0, 0x1e8e0, 0x1ea02, 0x1eb24,
      0x1ec47, 0x1ed6b, 0x1ee8f, 0x1efb4, 0x1f0d9, 0x1f1ff, 0x1f326, 0x1f44e,
      0x1f576, 0x1f69f, 0x1f7c9, 0x1f8f3, 0x1fa1e, 0x1fb4a, 0x1fc76, 0x1fda3,
      0x1fed1 }
   };

static uint32_t PITCH_GetScale(int32_t pitchoffset)
{
	uint32_t scale;
	 int32_t octaveshift;
	 int32_t noteshift;
	 int32_t note;
	 int32_t detune;

	if (pitchoffset == 0)
		return PitchTable[0][0];

	noteshift = pitchoffset % 1200;
	if (noteshift < 0)
		noteshift += 1200;

	note        = noteshift / 100;
	detune      = (noteshift % 100) / (100 / MAXDETUNE);
	octaveshift = (pitchoffset - noteshift) / 1200;

	if (detune < 0)
	{
		detune += (100 / MAXDETUNE);
		note--;
		if (note < 0)
		{
			note += 12;
			octaveshift--;
		}
	}

	scale = PitchTable[note][detune];

	if (octaveshift < 0)
		scale >>= -octaveshift;
	else
		scale <<= octaveshift;

	return scale;
}


/*---------------------------------------------------------------------
   Function: MV_SetVoicePitch

   Sets the pitch for the specified voice.
---------------------------------------------------------------------*/

static void MV_SetVoicePitch(VoiceNode *voice, uint32_t rate, int32_t pitchoffset)
{
	voice->SamplingRate = rate;
	voice->RateScale    = (rate * PITCH_GetScale(pitchoffset)) / MV_MixRate;

	// Multiply by MixBufferSize - 1
	voice->FixedPointBufferSize = (voice->RateScale * MixBufferSize) - voice->RateScale;
}


/*---------------------------------------------------------------------
   Function: MV_GetVolumeTable

   Returns a pointer to the volume table associated with the specified
   volume.
---------------------------------------------------------------------*/

static int16_t *MV_GetVolumeTable(int32_t vol)
{
	int16_t *table;

	if (vol > 255)
		vol = 255;
	else if (vol < 0)
		vol = 0;

	vol = vol >> 2;

	table = (int16_t *) &MV_VolumeTable[vol];

	return table;
}


/*---------------------------------------------------------------------
   Function: MV_SetVoiceMixMode

   Selects which method should be used to mix the voice.
---------------------------------------------------------------------*/

#define T_SIXTEENBIT_STEREO 0
#define T_8BITS       1
#define T_MONO        2
#define T_LEFTQUIET   8
#define T_RIGHTQUIET  16
#define T_DEFAULT     T_SIXTEENBIT_STEREO

static void MV_SetVoiceMixMode(VoiceNode *voice)
{
	uint32_t flags;
	 int32_t test;

	flags = DisableInterrupts();

	test = T_DEFAULT;
	if (MV_Bits == 8)
		test |= T_8BITS;

	if (MV_Channels == 1)
		test |= T_MONO;
	else
	{
		if (IS_QUIET(voice->RightVolume))
			test |= T_RIGHTQUIET;
		else if (IS_QUIET(voice->LeftVolume))
			test |= T_LEFTQUIET;
	}

	switch (test)
	{
		case T_8BITS | T_MONO:
			voice->mix = MV_Mix8BitMono;
			break;

		case T_8BITS | T_LEFTQUIET:
			MV_LeftVolume = MV_RightVolume;
			voice->mix = MV_Mix8BitMono;
			break;

		case T_8BITS | T_RIGHTQUIET:
			voice->mix = MV_Mix8BitMono;
			break;

		case T_8BITS:
			voice->mix = MV_Mix8BitStereo;
			break;

		case T_MONO:
			voice->mix = MV_Mix16BitMono;
			break;

		case T_LEFTQUIET:
			MV_LeftVolume = MV_RightVolume;
			voice->mix = MV_Mix16BitMono;
			break;

		case T_RIGHTQUIET:
			voice->mix = MV_Mix16BitMono;
			break;

		case T_SIXTEENBIT_STEREO:
			voice->mix = MV_Mix16BitStereo;
			break;

		default:
			voice->mix = MV_Mix8BitMono;
	}

	RestoreInterrupts(flags);
}


/*---------------------------------------------------------------------
   Function: MV_SetVoiceVolume

   Sets the stereo and mono volume level of the voice associated
   with the specified handle.
---------------------------------------------------------------------*/

static void MV_SetVoiceVolume(VoiceNode *voice, int32_t vol, int32_t left, int32_t right)
{
	if (MV_Channels == 1)
	{
		left  = vol;
		right = vol;
	}

	if (MV_SwapLeftRight)
	{
		// SBPro uses reversed panning
		voice->LeftVolume  = MV_GetVolumeTable(right);
		voice->RightVolume = MV_GetVolumeTable(left);
	} else {
		voice->LeftVolume  = MV_GetVolumeTable(left);
		voice->RightVolume = MV_GetVolumeTable(right);
	}

	MV_SetVoiceMixMode(voice);
}


/*---------------------------------------------------------------------
   Function: MV_SetOrigin

   Sets the stereo and mono volume level of the voice associated with the specified handle.
   Sets the pitch for the voice associated with the specified handle.
---------------------------------------------------------------------*/

void MV_SetOrigin(int32_t handle, int32_t pitchoffset, int32_t vol, int32_t left, int32_t right)
{
	VoiceNode *voice;

	if (!MV_Installed)
	{
		MV_SetErrorCode(MV_NotInstalled);
		return;
	}

	voice = MV_GetVoice(handle);
	if (voice == NULL)
		MV_SetErrorCode(MV_VoiceNotFound);
	else {
		MV_SetVoicePitch(voice, voice->SamplingRate, pitchoffset);
		MV_SetVoiceVolume(voice, vol, left, right);
	}
}


/*---------------------------------------------------------------------
   Function: MV_SetMixMode

   Prepares Multivoc to play stereo of mono digitized sounds.
---------------------------------------------------------------------*/

static void MV_SetMixMode(void)
{
	if (!MV_Installed)
	{
		MV_SetErrorCode(MV_NotInstalled);
		return;
	}

	switch(MV_SoundCard)
	{
		case AHW_SOUND_BLASTER:
			MV_MixMode = BLASTER_SetMixMode(STEREO | SIXTEEN_BIT);
			break;
	}

	MV_Channels = MV_MixMode & STEREO      ?  2 : 1;
	MV_Bits     = MV_MixMode & SIXTEEN_BIT ? 16 : 8;

	MV_BuffShift  = 7 + MV_Channels;
	MV_SampleSize = MV_Channels;

	if (MV_Bits == 8)
		MV_Silence = SILENCE_8BIT;
	else
	{
		MV_Silence     = SILENCE_16BIT;
		MV_BuffShift  += 1;
		MV_SampleSize *= 2;
	}

	MV_BufferSize = MixBufferSize * MV_SampleSize;
	MV_NumberOfBuffers = TotalBufferSize / MV_BufferSize;

	MV_RightChannelOffset = MV_SampleSize / 2;
}


/*---------------------------------------------------------------------
   Function: MV_StartPlayback

   Starts the sound playback engine.
---------------------------------------------------------------------*/

static int32_t MV_StartPlayback(void)
{
	int32_t status;
	int32_t buffer;

	// Initialize the buffers
	ClearBuffer_DW(MV_MixBuffer[0], MV_Silence, TotalBufferSize >> 2);
	for (buffer = 0; buffer < MV_NumberOfBuffers; buffer++)
		MV_BufferEmpty[buffer] = true;

	// Set the mix buffer variables
	MV_MixPage = 1;

	// Start playback
	switch (MV_SoundCard)
	{
		case AHW_SOUND_BLASTER:
			status = BLASTER_BeginBufferedPlayback(MV_MixBuffer[0], TotalBufferSize, MV_NumberOfBuffers, MV_RequestedMixRate, MV_MixMode, MV_ServiceVoc);
			if (status != BLASTER_Ok)
			{
				MV_SetErrorCode(MV_BlasterError);
				return MV_Error;
			}

			MV_MixRate = BLASTER_GetPlaybackRate();
			MV_DMAChannel = BLASTER_GetDMAChannel();
			break;
	}

	return MV_Ok;
}


/*---------------------------------------------------------------------
   Function: MV_StopPlayback

   Stops the sound playback engine.
---------------------------------------------------------------------*/

static void MV_StopPlayback(void)
{
	VoiceNode   *voice;
	VoiceNode   *next;
	uint32_t    flags;

	// Stop sound playback
	switch (MV_SoundCard)
	{
		case AHW_SOUND_BLASTER:
			BLASTER_StopPlayback();
			break;
	}

	// Make sure all callbacks are done.
	flags = DisableInterrupts();

	voice = VoiceList.start;
	while (voice != NULL)
	{
		next = voice->next;

		MV_StopVoice(voice);

		voice = next;
	}

	RestoreInterrupts(flags);
}


/*---------------------------------------------------------------------
   Function: MV_PlayRaw

   Begin playback of sound data with the given sound levels and
   priority.
---------------------------------------------------------------------*/

int32_t MV_PlayRaw(uint8_t *ptr, uint32_t length, uint32_t rate, int32_t pitchoffset, int32_t vol, int32_t left, int32_t right, int32_t priority)
{
	VoiceNode *voice;

	if (!MV_Installed)
	{
		MV_SetErrorCode(MV_NotInstalled);
		return MV_Error;
	}

	// Request a voice from the voice pool
	voice = MV_AllocVoice(priority);
	if (voice == NULL)
	{
		MV_SetErrorCode(MV_NoVoices);
		return MV_Error;
	}

	voice->Playing     = true;
	voice->NextBlock   = ptr;
	voice->position    = 0;
	voice->BlockLength = length;
	voice->length      = 0;
	voice->next        = NULL;
	voice->prev        = NULL;
	voice->priority    = priority;

	MV_SetVoicePitch(voice, rate, pitchoffset);
	MV_SetVoiceVolume(voice, vol, left, right);
	MV_PlayVoice(voice);

	return voice->handle;
}


/*---------------------------------------------------------------------
   Function: MV_SetVolume

   Sets the volume of digitized sound playback.

   Create the table used to convert sound data to a specific volume
   level.
---------------------------------------------------------------------*/

static void MV_SetVolume(void)
{
	int32_t volume;
	int32_t val;
	int32_t i;

	for (volume = 0; volume < 128; volume++)
	{
		MV_HarshClipTable[volume]       = 0;
		MV_HarshClipTable[volume + 384] = 255;
	}
	for (volume = 0; volume < 256; volume++)
		MV_HarshClipTable[volume + 128] = volume;

	// For each volume level, create a translation table with the
	// appropriate volume calculated.
	for (volume = 0; volume <= MV_MaxVolume; volume++)
	{
		if (MV_Bits == 16)
		{
			for (i = 0; i < 65536; i += 256)
			{
				val   = i - 0x8000;
				val  *= volume;
				val  /= MV_MaxVolume;
				MV_VolumeTable[volume][i / 256] = val;
			}
		} else {
			for (i = 0; i < 256; i++)
			{
				val   = i - 0x80;
				val  *= volume;
				val  /= MV_MaxVolume;
				MV_VolumeTable[volume][i] = val;
			}
		}
	}
}


/*---------------------------------------------------------------------
   Function: MV_TestPlayback

   Checks if playback has started.
---------------------------------------------------------------------*/

static int32_t MV_TestPlayback(void)
{
	uint32_t flags;
	int32_t  time;
	int32_t  start;
	int32_t  status;

	flags = DisableInterrupts();
	_enable();

	status = MV_Error;
	start  = MV_MixPage;
	time   = clock() + CLOCKS_PER_SEC * 2;

	while (clock() < time)
	{
		if (MV_MixPage != start)
		{
			status = MV_Ok;
			break;
		}
	}

	RestoreInterrupts(flags);

	if (status != MV_Ok)
		MV_SetErrorCode(MV_DMAFailure);

	return status;
}


static boolean DPMI_GetDOSMemory(void **ptr, uint16_t *selector, uint32_t length)
{
	union REGS regs;

#if defined __DJGPP__ || defined __CCDL__ || defined __WATCOMC__
	regs.w.ax = 0x0100;
	regs.w.bx = (length + 15) / 16;
#elif defined __DMC__
	regs.x.ax = 0x0100;
	regs.x.bx = (length + 15) / 16;
#endif

	int386(0x31, &regs, &regs);

#if defined __DJGPP__ || defined __CCDL__ || defined __WATCOMC__
	if (!regs.w.cflag)
#elif defined __DMC__
	if (!regs.x.cflag)
#endif
	{
#if defined __DJGPP__ || defined __CCDL__ || defined __WATCOMC__
		uint32_t eax = regs.w.ax;
		uint16_t  dx = regs.w.dx;
#elif defined __DMC__
		uint32_t eax = regs.x.ax;
		uint16_t  dx = regs.x.dx;
#endif

		*ptr      = (void *)(eax << 4);
		*selector = dx;
		return false;
	} else {
		return true;
	}
}


static void DPMI_FreeDOSMemory(uint16_t selector)
{
	union REGS regs;
#if defined __DJGPP__ || defined __CCDL__ || defined __WATCOMC__
	regs.w.ax = 0x0101;
	regs.w.dx = selector;
#elif __DMC__
	regs.x.ax = 0x0101;
	regs.x.dx = selector;
#endif
	int386(0x31, &regs, &regs);
}


/*---------------------------------------------------------------------
   Function: MV_Init

   Perform the initialization of variables and memory used by
   Multivoc.
---------------------------------------------------------------------*/

void MV_Init(int32_t soundcard, int32_t MixRate, int32_t Voices)
{
	uint8_t *ptr;
	int32_t  status;
	int32_t  buffer;
	int32_t  index;

	if (MV_Installed)
		MV_Shutdown();

	MV_SetErrorCode(MV_Ok);

	MV_TotalMemory = Voices * sizeof(VoiceNode) + sizeof(HARSH_CLIP_TABLE_8);
	ptr = malloc(MV_TotalMemory);
	if (!ptr)
	{
		MV_SetErrorCode(MV_NoMem);
		return;
	}

	MV_Voices = (VoiceNode *)ptr;
	MV_HarshClipTable = ptr + (MV_TotalMemory - sizeof(HARSH_CLIP_TABLE_8));

	// Set number of voices before calculating volume table
	MV_MaxVoices = Voices;

	LL_Reset(&VoiceList);
	LL_Reset(&VoicePool);

	for (index = 0; index < Voices; index++)
		LL_AddToTail(VoiceNode, &VoicePool, &MV_Voices[index]);

	// Allocate mix buffer within 1st megabyte
	status = DPMI_GetDOSMemory((void **)&ptr, &MV_BufferDescriptor, 2 * TotalBufferSize);
	if (status)
	{
		free(MV_Voices);
		MV_Voices = NULL;

		MV_TotalMemory = 0;

		MV_SetErrorCode(MV_NoMem);
		return;
	}

	MV_SwapLeftRight = false;

	// Initialize the sound card
	switch (soundcard)
	{
		case AHW_SOUND_BLASTER:
			MV_SwapLeftRight = BLASTER_IsSwapLeftRight();
			break;

		default:
			MV_SetErrorCode(MV_UnsupportedCard);
			break;
	}

	if (MV_ErrorCode != MV_Ok)
	{
		status = MV_ErrorCode;

		free(MV_Voices);
		MV_Voices = NULL;

		MV_TotalMemory = 0;

		DPMI_FreeDOSMemory(MV_BufferDescriptor);

		MV_SetErrorCode(status);
		return;
	}

	MV_SoundCard    = soundcard;
	MV_Installed    = true;

	// Set the sampling rate
	MV_RequestedMixRate = MixRate;

	// Set Mixer to play stereo digitized sound
	MV_SetMixMode();

	// Make sure we don't cross a physical page
	ptr += __djgpp_conventional_base;
	if (((uint32_t)ptr & 0xffff) + TotalBufferSize > 0x10000)
		ptr = (uint8_t *)(((uint32_t)ptr & 0xff0000) + 0x10000);

	MV_MixBuffer[MV_NumberOfBuffers] = ptr;
	for (buffer = 0; buffer < MV_NumberOfBuffers; buffer++)
	{
		MV_MixBuffer[buffer] = ptr;
		ptr += MV_BufferSize;
	}

	MV_SetVolume();

	// Start the playback engine
	status = MV_StartPlayback();
	if (status != MV_Ok)
	{
		// Preserve error code while we shutdown.
		status = MV_ErrorCode;
		MV_Shutdown();
		MV_SetErrorCode(status);
		return;
	}

	if (MV_TestPlayback() != MV_Ok)
	{
		status = MV_ErrorCode;
		MV_Shutdown();
		MV_SetErrorCode(status);
	}
}


/*---------------------------------------------------------------------
   Function: MV_Shutdown

   Restore any resources allocated by Multivoc back to the system.
---------------------------------------------------------------------*/

void MV_Shutdown(void)
{
	int32_t  buffer;
	uint32_t flags;

	if (!MV_Installed)
		return;

	flags = DisableInterrupts();

	MV_KillAllVoices();

	MV_Installed = false;

	// Stop the sound playback engine
	MV_StopPlayback();

	// Shutdown the sound card
	switch (MV_SoundCard)
	{
		case AHW_SOUND_BLASTER:
			BLASTER_Shutdown();
			break;
	}

	RestoreInterrupts(flags);

	// Free any voices we allocated
	if (MV_Voices)
		free(MV_Voices);

	MV_Voices = NULL;

	MV_TotalMemory = 0;

	LL_Reset(&VoiceList);
	LL_Reset(&VoicePool);

	MV_MaxVoices = 1;

	// Release the descriptor from our mix buffer
	DPMI_FreeDOSMemory(MV_BufferDescriptor);
	for (buffer = 0; buffer < NumberOfBuffers; buffer++)
		MV_MixBuffer[buffer] = NULL;
}
