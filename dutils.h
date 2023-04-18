//
// Copyright (C) 1993-1996 Id Software, Inc.
// Copyright (C) 2023 Frenkel Smeijers
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef __DUTILSH__
#define __DUTILSH__

//
// CHEAT SEQUENCE PACKAGE
//

#define SCRAMBLE(a) ((((a)&1)<<7) + (((a)&2)<<5) + ((a)&4) + (((a)&8)<<1) \
 + (((a)&16)>>1) + ((a)&32) + (((a)&64)>>5) + (((a)&128)>>7))

typedef struct
{
  unsigned char *sequence, *p;
} cheatseq_t;

boolean cht_CheckCheat(cheatseq_t *cht, char key);
void cht_GetParam(cheatseq_t *cht, char *buffer);

//
//                       SCREEN WIPE PACKAGE
//

void wipe_StartScreen(void);
void wipe_EndScreen(void);
boolean wipe_ScreenWipe(int ticks);

#endif
