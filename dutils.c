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

// DUtils.c
#include "doomdef.h"
#include "dutils.h"

//
// CHEAT SEQUENCE PACKAGE
//

static boolean firsttime = true;
static uint8_t cheat_xlate_table[256];


//
// Called in st_stuff module, which handles the input.
// Returns true if the cheat was successful, false if failed.
//
boolean cht_CheckCheat(cheatseq_t *cht, char key)
{
  int32_t i;
  boolean rc = false;

  if (firsttime)
  {
    firsttime = false;
    for (i=0;i<256;i++) cheat_xlate_table[i] = SCRAMBLE(i);
  }

  if (!cht->p)
    cht->p = cht->sequence; // initialize if first time

  if (*cht->p == 0)
    *(cht->p++) = key;
  else if
    (cheat_xlate_table[(uint8_t)key] == *cht->p) cht->p++;
  else
    cht->p = cht->sequence;

  if (*cht->p == 1)
    cht->p++;
  else if (*cht->p == 0xff) // end of sequence character
  {
    cht->p = cht->sequence;
    rc = true;
  }

  return rc;
}

void cht_GetParam(cheatseq_t *cht, char *buffer)
{
  uint8_t *p, c;

  p = cht->sequence;
  while (*(p++) != 1)
  {
  }
    
  do
  {
    c = *p;
    *(buffer++) = c;
    *(p++) = 0;
  }
  while (c && *p!=0xff );

  if (*p==0xff)
    *buffer = 0;
}

//
//                       SCREEN WIPE PACKAGE
//

// when false, stop the wipe
static boolean go = false;

static byte *wipe_scr_start, *wipe_scr_end, *wipe_scr;


static void wipe_shittyColMajorXform(int16_t *array)
{
  int32_t x, y;
  int16_t *dest;

  dest = (int16_t*) Z_Malloc(SCREENWIDTH*SCREENHEIGHT, PU_STATIC, 0);

  for(y=0;y<SCREENHEIGHT;y++)
    for(x=0;x<SCREENWIDTH/2;x++)
      dest[x*SCREENHEIGHT+y] = array[y*SCREENWIDTH/2+x];

  memcpy(array, dest, SCREENWIDTH*SCREENHEIGHT);

  Z_Free(dest);
}

static int32_t *y;

static void wipe_initMelt(void)
{
  int32_t i, r;
    
  // copy start screen to main screen
  memcpy(wipe_scr, wipe_scr_start, SCREENWIDTH*SCREENHEIGHT); 
  // makes this wipe faster (in theory)
  // to have stuff in column-major format
  wipe_shittyColMajorXform((int16_t*)wipe_scr_start);
  wipe_shittyColMajorXform((int16_t*)wipe_scr_end);
  // setup initial column positions
  // (y<0 => not ready to scroll yet)
  y = (int32_t *) Z_Malloc(SCREENWIDTH*sizeof(int32_t), PU_STATIC, 0);
  y[0] = -(M_Random()%16);
  for (i=1;i<SCREENWIDTH;i++)
  {
    r = (M_Random()%3) - 1;
    y[i] = y[i-1] + r;
    if (y[i] > 0) y[i] = 0;
    else if (y[i] == -16) y[i] = -15;
  }
}

static boolean wipe_doMelt(int32_t ticks)
{
  int32_t i, j, dy, idx;
  int16_t *s, *d;
  boolean done = true;

  while (ticks--)
  {
    for (i=0;i<SCREENWIDTH/2;i++)
    {
      if (y[i]<0)
      {
	y[i]++; done = false;
      }
      else if (y[i] < SCREENHEIGHT)
      {
	dy = (y[i] < 16) ? y[i]+1 : 8;
	if (y[i]+dy >= SCREENHEIGHT) dy = SCREENHEIGHT - y[i];
	s = &((int16_t *)wipe_scr_end)[i*SCREENHEIGHT+y[i]];
	d = &((int16_t *)wipe_scr)[y[i]*SCREENWIDTH/2+i];
	idx = 0;
	for (j=dy;j;j--)
	{
	  d[idx] = *(s++);
	  idx += SCREENWIDTH/2;
	}
	y[i] += dy;
	s = &((int16_t *)wipe_scr_start)[i*SCREENHEIGHT];
	d = &((int16_t *)wipe_scr)[y[i]*SCREENWIDTH/2+i];
	idx = 0;
	for (j=SCREENHEIGHT-y[i];j;j--)
	{
	  d[idx] = *(s++);
	  idx += SCREENWIDTH/2;
	}
	done = false;
      }
    }
  }

  return done;
}

void wipe_StartScreen(void)
{
  wipe_scr_start = screens[2];
  I_ReadScreen(wipe_scr_start);
}

void wipe_EndScreen(void)
{
  wipe_scr_end = screens[3];
  I_ReadScreen(wipe_scr_end);
  V_DrawBlock(wipe_scr_start); // restore start scr.
}

boolean wipe_ScreenWipe(int32_t ticks)
{
  boolean rc;

  void V_MarkRect(int32_t, int32_t, int32_t, int32_t);

  // initial stuff
  if (!go)
  {
    go = true;
    wipe_scr = screens[0];
    wipe_initMelt();
  }

  // do a piece of wipe-in
  V_MarkRect(0, 0, SCREENWIDTH, SCREENHEIGHT);
  rc = wipe_doMelt(ticks);

  // final stuff
  if (rc)
  {
    go = false;
    Z_Free(y);
  }

  return !go;
}
