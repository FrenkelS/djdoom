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

// AM_map.c

#include "doomdef.h"
#include "p_local.h"
#include "dutils.h"
#include "am_map.h"
#include "am_data.h"

static int32_t cheating = 0;
static boolean grid = false;

boolean    automapactive = false;
static byte *fb; // pseudo-frame buffer

static mpoint_t m_paninc; // how far the window pans each tic (map coords)
static fixed_t mtof_zoommul; // how far the window zooms in each tic (map coords)
static fixed_t ftom_zoommul; // how far the window zooms in each tic (fb coords)

static fixed_t m_x, m_y;   // LL x,y where the window is on the map (map coords)
static fixed_t m_x2, m_y2; // UR x,y where the window is on the map (map coords)

// width/height of window on map (map coords)
static fixed_t m_w, m_h;
static fixed_t min_x, min_y; // based on level size
static fixed_t max_x, max_y; // based on level size
static fixed_t max_w, max_h; // max_x-min_x, max_y-min_y
static fixed_t min_w, min_h; // based on player size
static fixed_t min_scale_mtof; // used to tell when to stop zooming out
static fixed_t max_scale_mtof; // used to tell when to stop zooming in

// old stuff for recovery later
static fixed_t old_m_w, old_m_h;
static fixed_t old_m_x, old_m_y;

// old location used by the Follower routine
static mpoint_t f_oldloc;

// used by MTOF to scale from map-to-frame-buffer coords
static fixed_t scale_mtof = INITSCALEMTOF;
// used by FTOM to scale from frame-buffer-to-map coords (=1/scale_mtof)
static fixed_t scale_ftom;

static player_t *plr; // the player represented by an arrow

static patch_t *marknums[10]; // numbers used for marking by the automap
static mpoint_t markpoints[AM_NUMMARKPOINTS]; // where the points are
static int32_t markpointnum = 0; // next point to be assigned

static boolean followplayer = true; // specifies whether to follow the player around

#if APPVER_CHEX
static uint8_t cheat_amap_seq[] =
{
  0xea, 0x32, 0xa6, 0x6a, 0x6a, 0xb2, 0x36, 0x36, 0xff
};
#else
static uint8_t cheat_amap_seq[] = { 0xb2, 0x26, 0x26, 0x2e, 0xff };
#endif
static cheatseq_t cheat_amap = { cheat_amap_seq, 0 };

extern boolean viewactive;
extern boolean singledemo;


static void AM_activateNewScale(void)
{
  m_x += m_w/2;
  m_y += m_h/2;
  m_w = FTOM(SCREENWIDTH);
  m_h = FTOM(SCREENHEIGHT-ST_HEIGHT);
  m_x -= m_w/2;
  m_y -= m_h/2;
  m_x2 = m_x + m_w;
  m_y2 = m_y + m_h;
}

static void AM_saveScaleAndLoc(void)
{
  old_m_x = m_x;
  old_m_y = m_y;
  old_m_w = m_w;
  old_m_h = m_h;
}

static void AM_restoreScaleAndLoc(void)
{

  m_w = old_m_w;
  m_h = old_m_h;
  if (!followplayer)
  {
    m_x = old_m_x;
    m_y = old_m_y;
  } else {
    m_x = plr->mo->x - m_w/2;
    m_y = plr->mo->y - m_h/2;
  }
  m_x2 = m_x + m_w;
  m_y2 = m_y + m_h;

  // Change the scaling multipliers
  scale_mtof = FixedDiv(SCREENWIDTH<<FRACBITS, m_w);
  scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
}

// adds a marker at the current location

static void AM_addMark(void)
{
  markpoints[markpointnum].x = m_x + m_w/2;
  markpoints[markpointnum].y = m_y + m_h/2;
  markpointnum = (markpointnum + 1) % AM_NUMMARKPOINTS;

}

static void AM_findMinMaxBoundaries(void)
{
  int32_t i;
  fixed_t a, b;

  min_x = min_y = MAXINT;
  max_x = max_y = -MAXINT;
  for (i=0;i<numvertexes;i++)
  {
    if (vertexes[i].x < min_x) min_x = vertexes[i].x;
    else if (vertexes[i].x > max_x) max_x = vertexes[i].x;
    if (vertexes[i].y < min_y) min_y = vertexes[i].y;
    else if (vertexes[i].y > max_y) max_y = vertexes[i].y;
  }
  max_w = max_x - min_x;
  max_h = max_y - min_y;
  min_w = 2*PLAYERRADIUS;
  min_h = 2*PLAYERRADIUS;

  a = FixedDiv(SCREENWIDTH<<FRACBITS, max_w);
  b = FixedDiv((SCREENHEIGHT-ST_HEIGHT)<<FRACBITS, max_h);
  min_scale_mtof = a < b ? a : b;

  max_scale_mtof = FixedDiv((SCREENHEIGHT-ST_HEIGHT)<<FRACBITS, 2*PLAYERRADIUS);

}

static void AM_changeWindowLoc(void)
{
  if (m_paninc.x || m_paninc.y)
  {
    followplayer = false;
    f_oldloc.x = MAXINT;
  }

  m_x += m_paninc.x;
  m_y += m_paninc.y;

  if (m_x + m_w/2 > max_x)
    m_x = max_x - m_w/2;
  else if (m_x + m_w/2 < min_x)
    m_x = min_x - m_w/2;

  if (m_y + m_h/2 > max_y)
    m_y = max_y - m_h/2;
  else if (m_y + m_h/2 < min_y)
    m_y = min_y - m_h/2;

  m_x2 = m_x + m_w;
  m_y2 = m_y + m_h;
}

static void AM_initVariables(void)
{
  int32_t pnum;
  static event_t st_notify = { ev_keyup, AM_MSGENTERED };

  automapactive = true;
  fb = screens[0];

  f_oldloc.x = MAXINT;

  m_paninc.x = m_paninc.y = 0;
  ftom_zoommul = FRACUNIT;
  mtof_zoommul = FRACUNIT;

  m_w = FTOM(SCREENWIDTH);
  m_h = FTOM(SCREENHEIGHT-ST_HEIGHT);

  // find player to center on initially
  pnum = consoleplayer;
  if (!playeringame[pnum])
    for (pnum=0;pnum<MAXPLAYERS;pnum++) if (playeringame[pnum]) break;
  plr = &players[pnum];
  m_x = plr->mo->x - m_w/2;
  m_y = plr->mo->y - m_h/2;
  AM_changeWindowLoc();

  // for saving & restoring
  old_m_x = m_x;
  old_m_y = m_y;
  old_m_w = m_w;
  old_m_h = m_h;

  // inform the status bar of the change
  ST_Responder(&st_notify);
}

static void AM_loadPics(void)
{
  int32_t i;
  char namebuf[9];
  for (i=0;i<10;i++)
  {
    sprintf(namebuf, "AMMNUM%d", i);
    marknums[i] = W_CacheLumpName(namebuf, PU_STATIC);
  }
}

static void AM_unloadPics(void)
{
  int32_t i;
  for (i=0;i<10;i++) Z_ChangeTag(marknums[i], PU_CACHE);

}

static void AM_clearMarks(void)
{
  int32_t i;
  for (i=0;i<AM_NUMMARKPOINTS;i++) markpoints[i].x = -1; // means empty
  markpointnum = 0;
}

// should be called at the start of every level
// right now, i figure it out myself

static void AM_LevelInit(void)
{
  AM_clearMarks();

  AM_findMinMaxBoundaries();
  scale_mtof = FixedDiv(min_scale_mtof, (int32_t) (0.7*FRACUNIT));
  if (scale_mtof > max_scale_mtof) scale_mtof = min_scale_mtof;
  scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
}

static boolean stopped = true;

void AM_Stop (void)
{
  static event_t st_notify = { 0, ev_keyup, AM_MSGEXITED };

  AM_unloadPics();
  automapactive = false;
  ST_Responder(&st_notify);
  stopped = true;
}

static void AM_Start (void)
{
  static int32_t lastlevel = -1, lastepisode = -1;

  if (!stopped) AM_Stop();
  stopped = false;
  if (lastlevel != gamemap || lastepisode != gameepisode)
  {
    AM_LevelInit();
    lastlevel = gamemap;
    lastepisode = gameepisode;
  }
  AM_initVariables();
  AM_loadPics();
}

// set the window scale to the maximum size

static void AM_minOutWindowScale(void)
{
  scale_mtof = min_scale_mtof;
  scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
  AM_activateNewScale();
}

// set the window scale to the minimum size

static void AM_maxOutWindowScale(void)
{
  scale_mtof = max_scale_mtof;
  scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
  AM_activateNewScale();
}

boolean AM_Responder (event_t *ev)
{
  boolean rc;
  static int32_t bigstate=0;
  static char buffer[20];

  rc = false;
  if (!automapactive)
  {
    if (ev->type == ev_keydown && ev->data1 == AM_STARTKEY)
    {
      AM_Start ();
      viewactive = false;
//      viewactive = true;
      rc = true;
    }
  }
  else if (ev->type == ev_keydown)
  {

    rc = true;
    switch(ev->data1)
    {
      case AM_PANRIGHTKEY: // pan right
	if (!followplayer) m_paninc.x = FTOM(F_PANINC);
	else rc = false;
	break;
      case AM_PANLEFTKEY: // pan left
	if (!followplayer) m_paninc.x = -FTOM(F_PANINC);
	else rc = false;
	break;
      case AM_PANUPKEY: // pan up
	if (!followplayer) m_paninc.y = FTOM(F_PANINC);
	else rc = false;
	break;
      case AM_PANDOWNKEY: // pan down
	if (!followplayer) m_paninc.y = -FTOM(F_PANINC);
	else rc = false;
	break;
      case AM_ZOOMOUTKEY: // zoom out
	mtof_zoommul = M_ZOOMOUT;
	ftom_zoommul = M_ZOOMIN;
	break;
      case AM_ZOOMINKEY: // zoom in
	mtof_zoommul = M_ZOOMIN;
	ftom_zoommul = M_ZOOMOUT;
	break;
      case AM_ENDKEY:
	bigstate = 0;
	viewactive = true;
	AM_Stop ();
	break;
      case AM_GOBIGKEY:
	bigstate = !bigstate;
	if (bigstate)
	{
	  AM_saveScaleAndLoc();
	  AM_minOutWindowScale();
	}
	else AM_restoreScaleAndLoc();
	break;
      case AM_FOLLOWKEY:
	followplayer = !followplayer;
	f_oldloc.x = MAXINT;
	plr->message = followplayer ? AMSTR_FOLLOWON : AMSTR_FOLLOWOFF;
	break;
      case AM_GRIDKEY:
	grid = !grid;
	plr->message = grid ? AMSTR_GRIDON : AMSTR_GRIDOFF;
	break;
      case AM_MARKKEY:
	sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, markpointnum);
	plr->message = buffer;
  	AM_addMark();
  	break;
      case AM_CLEARMARKKEY:
  	AM_clearMarks();
	plr->message = AMSTR_MARKSCLEARED;
  	break;
      default:
	rc = false;
    }
#if (APPVER_DOOMREV < AV_DR_DM19)
    if(cht_CheckCheat(&cheat_amap, ev->data1))
#else
    if(!deathmatch && cht_CheckCheat(&cheat_amap, ev->data1))
#endif
    {
      rc = false;
      cheating = (cheating+1) % 3;
    }
  }

  else if (ev->type == ev_keyup)
  {
    rc = false;
    switch (ev->data1)
    {
      case AM_PANRIGHTKEY:
	if (!followplayer) m_paninc.x = 0;
	break;
      case AM_PANLEFTKEY:
	if (!followplayer) m_paninc.x = 0;
	break;
      case AM_PANUPKEY:
	if (!followplayer) m_paninc.y = 0;
	break;
      case AM_PANDOWNKEY:
	if (!followplayer) m_paninc.y = 0;
	break;
      case AM_ZOOMOUTKEY:
      case AM_ZOOMINKEY:
	mtof_zoommul = FRACUNIT;
	ftom_zoommul = FRACUNIT;
	break;
    }
  }

  return rc;

}

static void AM_changeWindowScale(void)
{

  // Change the scaling multipliers
  scale_mtof = FixedMul(scale_mtof, mtof_zoommul);
  scale_ftom = FixedDiv(FRACUNIT, scale_mtof);

  if (scale_mtof < min_scale_mtof) AM_minOutWindowScale();
  else if (scale_mtof > max_scale_mtof) AM_maxOutWindowScale();
  else AM_activateNewScale();
}

static void AM_doFollowPlayer(void)
{
  if (f_oldloc.x != plr->mo->x || f_oldloc.y != plr->mo->y)
  {
//  m_x = FTOM(MTOF(plr->mo->x - m_w/2));
//  m_y = FTOM(MTOF(plr->mo->y - m_h/2));
//  m_x = plr->mo->x - m_w/2;
//  m_y = plr->mo->y - m_h/2;
    m_x = FTOM(MTOF(plr->mo->x)) - m_w/2;
    m_y = FTOM(MTOF(plr->mo->y)) - m_h/2;
    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;

    f_oldloc.x = plr->mo->x;
    f_oldloc.y = plr->mo->y;
  }
}

void AM_Ticker (void)
{

  if (!automapactive) return;

  if (followplayer) AM_doFollowPlayer();

  // Change the zoom if necessary
  if (ftom_zoommul != FRACUNIT) AM_changeWindowScale();

  // Change x,y location
  if (m_paninc.x || m_paninc.y) AM_changeWindowLoc();
}

static void AM_clearFB(void)
{
  memset(fb, BACKGROUND, SCREENWIDTH*(SCREENHEIGHT-ST_HEIGHT));
}

// Based on Cohen-Sutherland clipping algorithm but with a slightly
// faster reject and precalculated slopes.  If I need the speed, will
// hash algorithm to the common cases.

static boolean AM_clipMline(mline_t *ml, fline_t *fl)
{
  enum { LEFT=1, RIGHT=2, BOTTOM=4, TOP=8 };
  register int32_t outcode1 = 0, outcode2 = 0, outside;
  fpoint_t tmp;
  int32_t dx, dy;

#define DOOUTCODE(oc, mx, my) \
  (oc) = 0; \
  if ((my) < 0) (oc) |= TOP; \
  else if ((my) >= SCREENHEIGHT-ST_HEIGHT) (oc) |= BOTTOM; \
  if ((mx) < 0) (oc) |= LEFT; \
  else if ((mx) >= SCREENWIDTH) (oc) |= RIGHT

  // do trivial rejects and outcodes
  if (ml->a.y > m_y2) outcode1 = TOP;
  else if (ml->a.y < m_y) outcode1 = BOTTOM;
  if (ml->b.y > m_y2) outcode2 = TOP;
  else if (ml->b.y < m_y) outcode2 = BOTTOM;
  if (outcode1 & outcode2) return false; // trivially outside

  if (ml->a.x < m_x) outcode1 |= LEFT;
  else if (ml->a.x > m_x2) outcode1 |= RIGHT;
  if (ml->b.x < m_x) outcode2 |= LEFT;
  else if (ml->b.x > m_x2) outcode2 |= RIGHT;
  if (outcode1 & outcode2) return false; // trivially outside

  // transform to frame-buffer coordinates.
  fl->a.x = CXMTOF(ml->a.x);
  fl->a.y = CYMTOF(ml->a.y);
  fl->b.x = CXMTOF(ml->b.x);
  fl->b.y = CYMTOF(ml->b.y);
  DOOUTCODE(outcode1, fl->a.x, fl->a.y);
  DOOUTCODE(outcode2, fl->b.x, fl->b.y);
  if (outcode1 & outcode2) return false;

  while (outcode1 | outcode2)
  {
    // may be partially inside box
    // find an outside point
    if (outcode1) outside = outcode1;
    else outside = outcode2;
    // clip to each side
    if (outside & TOP)
    {
      dy = fl->a.y - fl->b.y;
      dx = fl->b.x - fl->a.x;
      tmp.x = fl->a.x + (dx*(fl->a.y))/dy;
      tmp.y = 0;
    }
    else if (outside & BOTTOM)
    {
      dy = fl->a.y - fl->b.y;
      dx = fl->b.x - fl->a.x;
      tmp.x = fl->a.x + (dx*(fl->a.y-(SCREENHEIGHT-ST_HEIGHT)))/dy;
      tmp.y = SCREENHEIGHT-ST_HEIGHT-1;
    }
    else if (outside & RIGHT)
    {
      dy = fl->b.y - fl->a.y;
      dx = fl->b.x - fl->a.x;
      tmp.y = fl->a.y + (dy*(SCREENWIDTH-1 - fl->a.x))/dx;
      tmp.x = SCREENWIDTH-1;
    }
    else if (outside & LEFT)
    {
      dy = fl->b.y - fl->a.y;
      dx = fl->b.x - fl->a.x;
      tmp.y = fl->a.y + (dy*(-fl->a.x))/dx;
      tmp.x = 0;
    }
    if (outside == outcode1)
    {
      fl->a = tmp;
      DOOUTCODE(outcode1, fl->a.x, fl->a.y);
    } else {
      fl->b = tmp;
      DOOUTCODE(outcode2, fl->b.x, fl->b.y);
    }
    if (outcode1 & outcode2) return false; // trivially outside
  }

  return true;
}
#undef DOOUTCODE

// Classic Bresenham w/ whatever optimizations I need for speed

static void AM_drawFline(fline_t *fl, int32_t color)
{

  register int32_t x, y, dx, dy, sx, sy, ax, ay, d;
  static int32_t fuck = 0;

  	// For debugging only
  if (   fl->a.x < 0 || fl->a.x >= SCREENWIDTH
      || fl->a.y < 0 || fl->a.y >= SCREENHEIGHT-ST_HEIGHT
      || fl->b.x < 0 || fl->b.x >= SCREENWIDTH
      || fl->b.y < 0 || fl->b.y >= SCREENHEIGHT-ST_HEIGHT)
  {
    fprintf(stderr, "fuck %d \r", fuck++);
    return;
  }
  
  #define PUTDOT(xx,yy,cc) fb[(yy)*SCREENWIDTH+(xx)]=(cc)
  
  dx = fl->b.x - fl->a.x;
  ax = 2 * (dx<0 ? -dx : dx);
  sx = dx<0 ? -1 : 1;
  
  dy = fl->b.y - fl->a.y;
  ay = 2 * (dy<0 ? -dy : dy);
  sy = dy<0 ? -1 : 1;
  
  x = fl->a.x;
  y = fl->a.y;
  
  if (ax > ay)
  {
    d = ay - ax/2;
    while (1)
    {
      PUTDOT(x,y,color);
      if (x == fl->b.x) return;
      if (d>=0)
      {
	y += sy;
	d -= ax;
      }
      x += sx;
      d += ay;
    }
  } else {
    d = ax - ay/2;
    while (1)
    {
      PUTDOT(x, y, color);
      if (y == fl->b.y) return;
      if (d >= 0)
      {
	x += sx;
	d -= ay;
      }
      y += sy;
      d += ax;
    }
  }
}

static void AM_drawMline(mline_t *ml, int32_t color)
{
  static fline_t fl;

  if (AM_clipMline(ml, &fl))
    AM_drawFline(&fl, color); // draws it on frame buffer using fb coords

}

static void AM_drawGrid(void)
{
  fixed_t x, y;
  fixed_t start, end;
  mline_t ml;

  // Figure out start of vertical gridlines
  start = m_x;
  if ((start-bmaporgx)%(MAPBLOCKUNITS<<FRACBITS))
    start += (MAPBLOCKUNITS<<FRACBITS)
      - ((start-bmaporgx)%(MAPBLOCKUNITS<<FRACBITS));
  end = m_x + m_w;

  // draw vertical gridlines
  ml.a.y = m_y;
  ml.b.y = m_y+m_h;
  for (x=start; x<end; x+=(MAPBLOCKUNITS<<FRACBITS))
  {
    ml.a.x = x;
    ml.b.x = x;
    AM_drawMline(&ml, GRIDCOLORS);
  }

  // Figure out start of horizontal gridlines
  start = m_y;
  if ((start-bmaporgy)%(MAPBLOCKUNITS<<FRACBITS))
    start += (MAPBLOCKUNITS<<FRACBITS)
      - ((start-bmaporgy)%(MAPBLOCKUNITS<<FRACBITS));
  end = m_y + m_h;

  // draw horizontal gridlines
  ml.a.x = m_x;
  ml.b.x = m_x + m_w;
  for (y=start; y<end; y+=(MAPBLOCKUNITS<<FRACBITS))
  {
    ml.a.y = y;
    ml.b.y = y;
    AM_drawMline(&ml, GRIDCOLORS);
  }
}

static void AM_drawWalls(void)
{
  int32_t i;
  static mline_t l;

  for (i=0;i<numlines;i++)
  {
    l.a.x = lines[i].v1->x;
    l.a.y = lines[i].v1->y;
    l.b.x = lines[i].v2->x;
    l.b.y = lines[i].v2->y;
    if (cheating || (lines[i].flags & ML_MAPPED))
    {
      if ((lines[i].flags & LINE_NEVERSEE) && !cheating)
	continue;
      if (!lines[i].backsector)
      {
        AM_drawMline(&l, WALLCOLORS);
      } else {
	if (lines[i].special == 39)
	{ // teleporters
          AM_drawMline(&l, WALLCOLORS+WALLRANGE/2);
	} else if (lines[i].flags & ML_SECRET) // secret door
	{
	  if (cheating) AM_drawMline(&l, SECRETWALLCOLORS);
	  else AM_drawMline(&l, WALLCOLORS);
	} else if (lines[i].backsector->floorheight
		   != lines[i].frontsector->floorheight) {
	  AM_drawMline(&l, FDWALLCOLORS); // floor level change
	} else if (lines[i].backsector->ceilingheight
		   != lines[i].frontsector->ceilingheight) {
	  AM_drawMline(&l, CDWALLCOLORS); // ceiling level change
	} else if (cheating) {
	  AM_drawMline(&l, TSWALLCOLORS);
	}
      }
    } else if (plr->powers[pw_allmap])
    {
      if (!(lines[i].flags & LINE_NEVERSEE)) AM_drawMline(&l, GRAYS+3);
    }
  }

}

static void AM_rotate(fixed_t *x, fixed_t *y, angle_t a)
{
  fixed_t tmpx;

  tmpx = FixedMul(*x,finecosine[a>>ANGLETOFINESHIFT])
       - FixedMul(*y,finesine[a>>ANGLETOFINESHIFT]);
  *y   = FixedMul(*x,finesine[a>>ANGLETOFINESHIFT])
       + FixedMul(*y,finecosine[a>>ANGLETOFINESHIFT]);
  *x = tmpx;
}

static void AM_drawLineCharacter(mline_t *lineguy, int32_t lineguylines, fixed_t scale,
  angle_t angle, int32_t color, fixed_t x, fixed_t y)
{
  int32_t i;
  mline_t l;

  for (i=0;i<lineguylines;i++)
  {
    l.a.x = lineguy[i].a.x;
    l.a.y = lineguy[i].a.y;
    if (scale)
    {
      l.a.x = FixedMul(scale, l.a.x);
      l.a.y = FixedMul(scale, l.a.y);
    }
    if (angle) AM_rotate(&l.a.x, &l.a.y, angle);
    l.a.x += x;
    l.a.y += y;

    l.b.x = lineguy[i].b.x;
    l.b.y = lineguy[i].b.y;
    if (scale)
    {
      l.b.x = FixedMul(scale, l.b.x);
      l.b.y = FixedMul(scale, l.b.y);
    }
    if (angle) AM_rotate(&l.b.x, &l.b.y, angle);
    l.b.x += x;
    l.b.y += y;

    AM_drawMline(&l, color);
  }

}

static void AM_drawPlayers(void)
{

  int32_t i;
  player_t *p;
  static int32_t their_colors[] = { GREENS, GRAYS, BROWNS, REDS };
  int32_t their_color = -1;
  int32_t color;

  if (!netgame)
  {
    if (cheating) AM_drawLineCharacter(cheat_player_arrow, NUMCHEATPLYRLINES, 0,
      plr->mo->angle, WHITE, plr->mo->x, plr->mo->y);
    else AM_drawLineCharacter(player_arrow, NUMPLYRLINES, 0, plr->mo->angle,
      WHITE, plr->mo->x, plr->mo->y);
    return;
  }

  for (i=0;i<MAXPLAYERS;i++)
  {
    their_color++;
    p = &players[i];
    if (deathmatch && !singledemo && p != plr)
      continue;
    if (!playeringame[i]) continue;
    if (p->powers[pw_invisibility]) color = 246; // *close* to black
    else color = their_colors[their_color];
    AM_drawLineCharacter(player_arrow, NUMPLYRLINES, 0, p->mo->angle,
      color, p->mo->x, p->mo->y);
  }
}

static void AM_drawThings(void)
{
  int32_t i;
  mobj_t *t;

  for (i=0;i<numsectors;i++)
  {
    t = sectors[i].thinglist;
    while (t)
    {
      AM_drawLineCharacter(thintriangle_guy, NUMTHINTRIANGLEGUYLINES,
	16<<FRACBITS, t->angle, THINGCOLORS, t->x, t->y);
      t = t->snext;
    }
  }
}

static void AM_drawMarks(void)
{
  int32_t i, fx, fy;

  for (i=0;i<AM_NUMMARKPOINTS;i++)
  {
    if (markpoints[i].x != -1)
    {
      fx = CXMTOF(markpoints[i].x);
      fy = CYMTOF(markpoints[i].y);
      if (0 <= fx && fx <= SCREENWIDTH - 5 && 0 <= fy && fy <= SCREENHEIGHT-ST_HEIGHT - 6)
	V_DrawPatch(fx, fy, FB, marknums[i]);
    }
  }
}

static void AM_drawCrosshair(void)
{
  fb[(SCREENWIDTH*(SCREENHEIGHT-ST_HEIGHT+1))/2] = XHAIRCOLORS; // single point for now
}

void AM_Drawer(void)
{
  if (!automapactive) return;

  AM_clearFB();
  if (grid) AM_drawGrid();
  AM_drawWalls();
  AM_drawPlayers();
  if (cheating==2) AM_drawThings();
  AM_drawCrosshair();

  AM_drawMarks();

  V_MarkRect(0, 0, SCREENWIDTH, SCREENHEIGHT-ST_HEIGHT);
}
