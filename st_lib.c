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

// ST_lib.c
#include "doomdef.h"
#include "st_lib.h"


// in AM_map.c
extern boolean		automapactive; 

static patch_t *sttminus;

void STlib_init(void)
{
 sttminus = (patch_t *) W_CacheLumpName("STTMINUS", PU_STATIC);
}


void STlib_initNum (st_number_t *n, int32_t x, int32_t y, patch_t **pl, int32_t *num, boolean *on, int32_t width)
{
  n->x = x;
  n->y = y;
  n->oldnum = 0;
  n->width = width;
  n->num = num;
  n->on = on;
  n->p = pl;
}


static void STlib_drawNum (st_number_t *n)
{

  int32_t numdigits = n->width;
  int32_t num = *n->num;
    
  int32_t w = SHORT(n->p[0]->width);
  int32_t h = SHORT(n->p[0]->height);
  int32_t x = n->x;
    
  boolean neg;

  n->oldnum = *n->num;

  neg = num < 0;

  if (neg)
  {
    if (numdigits == 2 && num < -9)
      num = -9;
    else if (numdigits == 3 && num < -99)
      num = -99;
	
    num = -num;
  }

// clear the area
  x = n->x - numdigits*w;

  if (n->y - ST_Y < 0)
    I_Error("drawNum: n->y - ST_Y < 0");

  V_CopyRect(x, n->y - ST_Y, w*numdigits, h, n->y);

// if non-number, do not draw it
  if (num == 1994)
    return;

  x = n->x;

// in the special case of 0, you draw 0
  if (!num)
    V_DrawPatch(x - w, n->y, FG, n->p[ 0 ]);

// draw the new number
  while (num && numdigits--)
  {
    x -= w;
    V_DrawPatch(x, n->y, FG, n->p[ num % 10 ]);
    num /= 10;
  }

// draw a minus sign if necessary
  if (neg)
    V_DrawPatch(x - 8, n->y, FG, sttminus);
}


void STlib_updateNum (st_number_t *n)
{
  if (*n->on) STlib_drawNum(n);
}


void STlib_initPercent (st_percent_t *p, int32_t x, int32_t y, patch_t **pl, int32_t *num, boolean *on, patch_t *percent)
{
  STlib_initNum(&p->n, x, y, pl, num, on, 3);
  p->p = percent;
}




void STlib_updatePercent (st_percent_t *per, boolean refresh)
{
  if (refresh && *per->n.on)
    V_DrawPatch(per->n.x, per->n.y, FG, per->p);
    
  STlib_updateNum(&per->n);
}



void STlib_initMultIcon (st_multicon_t *mi, int32_t x, int32_t y, patch_t **il, int32_t *inum, boolean *on)
{
  mi->x = x;
  mi->y = y;
  mi->oldinum = -1;
  mi->inum = inum;
  mi->on = on;
  mi->p = il;
}



void STlib_updateMultIcon (st_multicon_t *mi, boolean refresh)
{
  int32_t w, h, x, y;

  if (*mi->on && (mi->oldinum != *mi->inum || refresh) && (*mi->inum!=-1))
  {
    if (mi->oldinum != -1)
    {
      x = mi->x - SHORT(mi->p[mi->oldinum]->leftoffset);
      y = mi->y - SHORT(mi->p[mi->oldinum]->topoffset);
      w = SHORT(mi->p[mi->oldinum]->width);
      h = SHORT(mi->p[mi->oldinum]->height);

      if (y - ST_Y < 0)
	I_Error("updateMultIcon: y - ST_Y < 0");

      V_CopyRect(x, y-ST_Y, w, h, y);
    }
    V_DrawPatch(mi->x, mi->y, FG, mi->p[*mi->inum]);
    mi->oldinum = *mi->inum;
  }
}



void STlib_initBinIcon (st_binicon_t *b, int32_t x, int32_t y, patch_t *i, boolean *val, boolean *on)
{
  b->x = x;
  b->y = y;
  b->oldval = 0;
  b->val = val;
  b->on = on;
  b->p = i;
}



void STlib_updateBinIcon (st_binicon_t *bi, boolean refresh)
{
  int32_t w, h, x, y;

  if (*bi->on && (bi->oldval != *bi->val || refresh))
  {
    x = bi->x - SHORT(bi->p->leftoffset);
    y = bi->y - SHORT(bi->p->topoffset);
    w = SHORT(bi->p->width);
    h = SHORT(bi->p->height);

    if (y - ST_Y < 0)
      I_Error("updateBinIcon: y - ST_Y < 0");

    if (*bi->val)
      V_DrawPatch(bi->x, bi->y, FG, bi->p);
    else
      V_CopyRect(x, y-ST_Y, w, h, y);

    bi->oldval = *bi->val;
  }

}
