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

int32_t MV_VoicePlaying(int32_t handle);
void    MV_Kill(int32_t handle);
void    MV_SetPitch(int32_t handle, int32_t pitchoffset);
void    MV_SetPan(int32_t handle, int32_t vol, int32_t left, int32_t right);
int32_t MV_PlayRaw(uint8_t *ptr, uint32_t length, uint32_t rate, int32_t pitchoffset, int32_t vol, int32_t left, int32_t right, int32_t priority);
void    MV_SetVolume(void);
void    MV_Init(int32_t soundcard, int32_t MixRate, int32_t Voices);
void    MV_Shutdown(void);

#endif
