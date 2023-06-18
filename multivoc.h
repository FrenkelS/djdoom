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
   file:   MULTIVOC.H

   author: James R. Dose
   date:   December 20, 1993

   Public header for MULTIVOC.C

   (c) Copyright 1993 James R. Dose.  All Rights Reserved.
**********************************************************************/

#ifndef __MULTIVOC_H
#define __MULTIVOC_H

#define MV_MinVoiceHandle  1

extern int MV_ErrorCode;

enum MV_Errors
   {
   MV_Warning = -2,
   MV_Error   = -1,
   MV_Ok      = 0,
   MV_UnsupportedCard,
   MV_NotInstalled,
   MV_NoVoices,
   MV_NoMem,
   MV_VoiceNotFound,
   MV_BlasterError,
   MV_PasError,
   MV_SoundScapeError,
   MV_SoundSourceError,
   MV_DPMI_Error,
   MV_InvalidVOCFile,
   MV_InvalidWAVFile,
   MV_InvalidMixMode,
   MV_SoundSourceFailure,
   MV_IrqFailure,
   MV_DMAFailure,
   MV_DMA16Failure,
   MV_NullRecordFunction
   };

int  MV_VoicePlaying(int handle);
void MV_Kill(int handle);
void MV_SetPitch(int handle, int pitchoffset);
void MV_SetPan(int handle, int vol, int left, int right);
int  MV_PlayRaw(char *ptr, unsigned long length, unsigned rate, int pitchoffset, int vol, int left, int right, int priority, unsigned long callbackval);
void MV_SetVolume(int volume);
void MV_Init(int soundcard, int MixRate, int Voices, int numchannels, int samplebits);
void MV_Shutdown(void);

#endif
