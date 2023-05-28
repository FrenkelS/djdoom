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
   module: SNDSRC.H

   author: James R. Dose
   date:   March 26, 1994

   Public header for for SNDSRC.C

   (c) Copyright 1994 James R. Dose.  All Rights Reserved.
**********************************************************************/

#ifndef __SNDSRC_H
#define __SNDSRC_H

enum SS_ERRORS
   {
   SS_Warning = -2,
   SS_Error   = -1,
   SS_Ok      = 0,
   SS_NotFound,
   SS_NoSoundPlaying,
   SS_DPMI_Error
   };

#define SS_Port1 0x3bc
#define SS_Port2 0x378
#define SS_Port3 0x278

#define SS_DefaultPort 0x378
#define SS_SampleRate  7000
#define SS_DMAChannel  -1

void  SS_StopPlayback( void );
int   SS_BeginBufferedPlayback( char *BufferStart, int BufferSize, int NumDivisions, void ( *CallBackFunc )( void ) );
int   SS_SetMixMode( int mode );
int   SS_Init( int soundcard );
void  SS_Shutdown( void );

#endif
