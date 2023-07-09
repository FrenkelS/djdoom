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

#include <stdint.h>

int  MV_VoicePlaying(int handle);
void MV_Kill(int handle);
void MV_SetPitch(int handle, int pitchoffset);
void MV_SetPan(int handle, int vol, int left, int right);
int  MV_PlayRaw(uint8_t *ptr, unsigned long length, unsigned rate, int pitchoffset, int vol, int left, int right, int priority);
void MV_SetVolume(void);
void MV_Init(int soundcard, int MixRate, int Voices);
void MV_Shutdown(void);

#endif
