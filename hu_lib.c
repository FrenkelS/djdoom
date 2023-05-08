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

// HU_lib.c
#include <ctype.h>
#include "doomdef.h"
#include "r_local.h"
#include "hu_lib.h"

// boolean : whether the screen is always erased
#define noterased viewwindowx

extern boolean automapactive;	// in AM_map.c

// clear a line of text
static void HUlib_clearTextLine(hu_textline_t *t)
{
  t->len = 0;
  t->l[0] = 0;
  t->needsupdate = true;
}

void HUlib_initTextLine(hu_textline_t *t, int32_t x, int32_t y, patch_t **f, int32_t sc)
{
  t->x = x;
  t->y = y;
  t->f = f;
  t->sc = sc;
  HUlib_clearTextLine(t);
}

void HUlib_addCharToTextLine(hu_textline_t *t, char ch)
{

  if (t->len != HU_MAXLINELENGTH)
  {
    t->l[t->len++] = ch;
    t->l[t->len] = 0;
    t->needsupdate = 4;
  }

}

static void HUlib_delCharFromTextLine(hu_textline_t *t)
{

  if (t->len)
  {
    t->l[--t->len] = 0;
    t->needsupdate = 4;
  }

}

void HUlib_drawTextLine(hu_textline_t *l, boolean drawcursor)
{

  int32_t i, w, x;
  unsigned char c;

// draw the new stuff
  x = l->x;
  for (i=0;i<l->len;i++)
  {
    c = toupper(l->l[i]);
    if (c != ' ' && c >= l->sc && c <= '_')
    {
      w = SHORT(l->f[c - l->sc]->width);
      if (x+w > SCREENWIDTH)
	break;
      V_DrawPatchDirect(x, l->y, l->f[c - l->sc]);
      x += w;
    }
    else
    {
      x += 4;
      if (x >= SCREENWIDTH)
        break;
    }
  }

// draw the cursor if requested
  if (drawcursor && x + SHORT(l->f['_' - l->sc]->width) <= SCREENWIDTH)
    V_DrawPatchDirect(x, l->y, l->f['_' - l->sc]);
}


// sorta called by HU_Erase and just better darn get things straight
void HUlib_eraseTextLine(hu_textline_t *l)
{
  int32_t lh, y, yoffset;

// Only erases when NOT in automap and the screen is reduced,
// and the text must either need updating or refreshing
// (because of a recent change back from the automap)

  if (!automapactive && viewwindowx && l->needsupdate)
  {
    lh = SHORT(l->f[0]->height) + 1;
    for (y=l->y,yoffset=y*SCREENWIDTH ; y<l->y+lh ; y++,yoffset+=SCREENWIDTH)
    {
      if (y < viewwindowy || y >= viewwindowy + viewheight)
	R_VideoErase(yoffset, SCREENWIDTH); // erase entire line
      else
      {
	R_VideoErase(yoffset, viewwindowx); // erase left border
	R_VideoErase(yoffset + viewwindowx + viewwidth, viewwindowx); // erase right border
      }
    }
  }

  if (l->needsupdate) l->needsupdate--;

}

void HUlib_initSText(hu_stext_t *s, int32_t x, int32_t y, int32_t h, patch_t **font, int32_t startchar, boolean *on)
{

  int32_t i;

  s->h = h;
  s->on = on;
  s->laston = true;
  s->cl = 0;
  for (i=0;i<h;i++)
    HUlib_initTextLine(&s->l[i], x, y - i*(SHORT(font[0]->height)+1),
      font, startchar);

}

// add a new line
static void HUlib_addLineToSText(hu_stext_t *s)
{

  int32_t i;

  // add a clear line
  if (++s->cl == s->h)
    s->cl = 0;
  HUlib_clearTextLine(&s->l[s->cl]);

  // everything needs updating
  for (i=0 ; i<s->h ; i++)
    s->l[i].needsupdate = 4;

}

void HUlib_addMessageToSText (hu_stext_t *s, char *prefix, char *msg)
{
  HUlib_addLineToSText(s);
  if (prefix)
    while (*prefix)
      HUlib_addCharToTextLine(&s->l[s->cl], *(prefix++));

  while (*msg)
    HUlib_addCharToTextLine(&s->l[s->cl], *(msg++));
}

void HUlib_drawSText(hu_stext_t *s)
{
  int32_t i, idx;
  hu_textline_t *l;

  if (!*s->on)
    return; // if not on, don't draw

  // draw everything
  for (i=0 ; i<s->h ; i++)
  {
    idx = s->cl - i;
    if (idx < 0)
      idx += s->h; // handle queue of lines
	
    l = &s->l[idx];

    // need a decision made here on whether to skip the draw
    HUlib_drawTextLine(l, false); // no cursor, please
  }

}

void HUlib_eraseSText(hu_stext_t *s)
{

  int32_t i;

  for (i=0 ; i<s->h ; i++)
  {
    if (s->laston && !*s->on)
      s->l[i].needsupdate = 4;
    HUlib_eraseTextLine(&s->l[i]);
  }
  s->laston = *s->on;

}

void HUlib_initIText(hu_itext_t *it, int32_t x, int32_t y, patch_t **font, int32_t startchar, boolean *on)
{
  it->lm = 0; // default left margin is start of text
  it->on = on;
  it->laston = true;
  HUlib_initTextLine(&it->l, x, y, font, startchar);
}


// The following deletion routines adhere to the left margin restriction
static void HUlib_delCharFromIText(hu_itext_t *it)
{
  if (it->l.len != it->lm)
    HUlib_delCharFromTextLine(&it->l);
}

// Resets left margin as well
void HUlib_resetIText(hu_itext_t *it)
{
  it->lm = 0;
  HUlib_clearTextLine(&it->l);
}

// wrapper function for handling general keyed input.
// returns true if it ate the key
boolean HUlib_keyInIText(hu_itext_t *it, unsigned char ch)
{

  if (ch >= ' ' && ch <= '_') 
    HUlib_addCharToTextLine(&it->l, (char) ch);
  else if (ch == KEY_BACKSPACE) 
    HUlib_delCharFromIText(it);
  else if (ch != KEY_ENTER) 
    return false; // did not eat key

  return true; // ate the key

}

void HUlib_drawIText(hu_itext_t *it)
{

  hu_textline_t *l = &it->l;

  if (!*it->on)
    return;
  HUlib_drawTextLine(l, true); // draw the line w/ cursor

}

void HUlib_eraseIText(hu_itext_t *it)
{
  if (it->laston && !*it->on)
    it->l.needsupdate = 4;
  HUlib_eraseTextLine(&it->l);
  it->laston = *it->on;
}
