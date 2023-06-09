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

// R_planes.c

#include "doomdef.h"
#include "r_local.h"

//
// sky mapping
//
int32_t			skyflatnum;
int32_t			skytexture;
static int32_t	skytexturemid;

//
// opening
//

static visplane_t		visplanes[MAXVISPLANES], *lastvisplane;
visplane_t		*floorplane, *ceilingplane;

static int16_t	openings[MAXOPENINGS];
int16_t			*lastopening;

//
// clip values are the solid pixel bounding the range
// floorclip starts out SCREENHEIGHT
// ceilingclip starts out -1
//
int16_t		floorclip[SCREENWIDTH];
int16_t		ceilingclip[SCREENWIDTH];

//
// spanstart holds the start of a plane span
// initialized to 0 at start
//
static int32_t			spanstart[SCREENHEIGHT];

//
// texture mapping
//
static lighttable_t	**planezlight;
static fixed_t		planeheight;

fixed_t		yslope[SCREENHEIGHT];
fixed_t		distscale[SCREENWIDTH];
static fixed_t		basexscale, baseyscale;

static fixed_t		cachedheight[SCREENHEIGHT];
static fixed_t		cacheddistance[SCREENHEIGHT];
static fixed_t		cachedxstep[SCREENHEIGHT];
static fixed_t		cachedystep[SCREENHEIGHT];


/*
================
=
= R_InitSkyMap
=
= Called whenever the view size changes
=
================
*/

void R_InitSkyMap (void)
{
	skyflatnum = R_FlatNumForName ("F_SKY1");
	skytexturemid = 100*FRACUNIT;
}


/*
================
=
= R_MapPlane
=
global vars:

planeheight
ds_source
basexscale
baseyscale
viewx
viewy

BASIC PRIMITIVE
================
*/

static void R_MapPlane (int32_t y, int32_t x1, int32_t x2)
{
	angle_t		angle;
	fixed_t		distance, length;
	uint32_t	index;
	
#ifdef RANGECHECK
	if (x2 < x1 || x1<0 || x2>=viewwidth || (uint32_t)y>viewheight)
		I_Error ("R_MapPlane: %i, %i at %i",x1,x2,y);
#endif

	if (planeheight != cachedheight[y])
	{
		cachedheight[y] = planeheight;
		distance = cacheddistance[y] = FixedMul (planeheight, yslope[y]);

		ds_xstep = cachedxstep[y] = FixedMul (distance,basexscale);
		ds_ystep = cachedystep[y] = FixedMul (distance,baseyscale);
	}
	else
	{
		distance = cacheddistance[y];
		ds_xstep = cachedxstep[y];
		ds_ystep = cachedystep[y];
	}
	
	length = FixedMul (distance,distscale[x1]);
	angle = (viewangle + xtoviewangle[x1])>>ANGLETOFINESHIFT;
	ds_xfrac = viewx + FixedMul(finecosine[angle], length);
	ds_yfrac = -viewy - FixedMul(finesine[angle], length);

	if (fixedcolormap)
		ds_colormap = fixedcolormap;
	else
	{
		index = distance >> LIGHTZSHIFT;
		if (index >= MAXLIGHTZ )
			index = MAXLIGHTZ-1;
		ds_colormap = planezlight[index];
	}
	
	ds_y = y;
	ds_x1 = x1;
	ds_x2 = x2;
	
	spanfunc ();		// high or low detail
}

//=============================================================================

/*
====================
=
= R_ClearPlanes
=
= At begining of frame
====================
*/

void R_ClearPlanes (void)
{
	int32_t	i;
	angle_t	angle;
	
//
// opening / clipping determination
//	
	for (i=0 ; i<viewwidth ; i++)
	{
		floorclip[i] = viewheight;
		ceilingclip[i] = -1;
	}

	lastvisplane = visplanes;
	lastopening = openings;
	
//
// texture calculation
//
	memset (cachedheight, 0, sizeof(cachedheight));	
	angle = (viewangle-ANG90)>>ANGLETOFINESHIFT;	// left to right mapping
	
	// scale will be unit scale at SCREENWIDTH/2 distance
	basexscale = FixedDiv (finecosine[angle],centerxfrac);
	baseyscale = -FixedDiv (finesine[angle],centerxfrac);
}



/*
===============
=
= R_FindPlane
=
===============
*/

visplane_t *R_FindPlane (fixed_t height, int32_t picnum, int32_t lightlevel)
{
	visplane_t *check;

	if(picnum == skyflatnum)
	{
		// all skies map together
		height = 0;
		lightlevel = 0;
	}

	for(check = visplanes; check < lastvisplane; check++)
	{
		if(height == check->height
		&& picnum == check->picnum
		&& lightlevel == check->lightlevel)
			break;
	}

	if(check < lastvisplane)
	{
		return(check);
	}

	if(lastvisplane-visplanes == MAXVISPLANES)
	{
		I_Error("R_FindPlane: no more visplanes");
	}

	lastvisplane++;
	check->height = height;
	check->picnum = picnum;
	check->lightlevel = lightlevel;
	check->minx = SCREENWIDTH;
	check->maxx = -1;
	memset(check->top,0xff,sizeof(check->top));
	return(check);
}

/*
===============
=
= R_CheckPlane
=
===============
*/

visplane_t *R_CheckPlane (visplane_t *pl, int32_t start, int32_t stop)
{
	int32_t			intrl, intrh;
	int32_t			unionl, unionh;
	int32_t			x;
	
	if (start < pl->minx)
	{
		intrl = pl->minx;
		unionl = start;
	}
	else
	{
		unionl = pl->minx;
		intrl = start;
	}
	
	if (stop > pl->maxx)
	{
		intrh = pl->maxx;
		unionh = stop;
	}
	else
	{
		unionh = pl->maxx;
		intrh = stop;
	}

	for (x=intrl ; x<= intrh ; x++)
		if (pl->top[x] != 0xff)
			break;

	if (x > intrh)
	{
		pl->minx = unionl;
		pl->maxx = unionh;
		return pl;			// use the same one
	}
	
// make a new visplane

	lastvisplane->height = pl->height;
	lastvisplane->picnum = pl->picnum;
	lastvisplane->lightlevel = pl->lightlevel;
	pl = lastvisplane++;
	pl->minx = start;
	pl->maxx = stop;
	memset (pl->top,0xff,sizeof(pl->top));
		
	return pl;
}



//=============================================================================

/*
================
=
= R_MakeSpans
=
================
*/

static void R_MakeSpans (int32_t x, int32_t t1, int32_t b1, int32_t t2, int32_t b2)
{
	while (t1 < t2 && t1<=b1)
	{
		R_MapPlane (t1,spanstart[t1],x-1);
		t1++;
	}
	while (b1 > b2 && b1>=t1)
	{
		R_MapPlane (b1,spanstart[b1],x-1);
		b1--;
	}
	
	while (t2 < t1 && t2<=b2)
	{
		spanstart[t2] = x;
		t2++;
	}
	while (b2 > b1 && b2>=t2)
	{
		spanstart[b2] = x;
		b2--;
	}
}



/*
================
=
= R_DrawPlanes
=
= At the end of each frame
================
*/

void R_DrawPlanes (void)
{
	visplane_t	*pl;
	int32_t		light;
	int32_t		x, stop;
	int32_t		angle;

#ifdef RANGECHECK
	if (ds_p - drawsegs > MAXDRAWSEGS)
		I_Error ("R_DrawPlanes: drawsegs overflow (%i)", ds_p - drawsegs);
	if (lastvisplane - visplanes > MAXVISPLANES)
		I_Error ("R_DrawPlanes: visplane overflow (%i)", lastvisplane - visplanes);
	if (lastopening - openings > MAXOPENINGS)
		I_Error ("R_DrawPlanes: opening overflow (%i)", lastopening - openings);
#endif

	for (pl = visplanes ; pl < lastvisplane ; pl++)
	{
		if (pl->minx > pl->maxx)
			continue;
	//
	// sky flat
	//
		if (pl->picnum == skyflatnum)
		{
			dc_iscale = pspriteiscale>>detailshift;
			dc_colormap = colormaps;// sky is allways drawn full bright
			dc_texturemid = skytexturemid;
			for (x=pl->minx ; x <= pl->maxx ; x++)
			{
				dc_yl = pl->top[x];
				dc_yh = pl->bottom[x];
				if (dc_yl <= dc_yh)
				{
					angle = (viewangle + xtoviewangle[x])>>ANGLETOSKYSHIFT;
					dc_x = x;
					dc_source = R_GetColumn(skytexture, angle);
					colfunc ();
				}
			}
			continue;
		}
		
	//
	// regular flat
	//
		ds_source = W_CacheLumpNum(firstflat +
			flattranslation[pl->picnum], PU_STATIC);
		planeheight = abs(pl->height-viewz);
		light = (pl->lightlevel >> LIGHTSEGSHIFT)+extralight;
		if (light >= LIGHTLEVELS)
			light = LIGHTLEVELS-1;
		if (light < 0)
			light = 0;
		planezlight = zlight[light];

		pl->top[pl->maxx+1] = 0xff;
		pl->top[pl->minx-1] = 0xff;
		
		stop = pl->maxx + 1;
		for (x=pl->minx ; x<= stop ; x++)
			R_MakeSpans (x,pl->top[x-1],pl->bottom[x-1]
			,pl->top[x],pl->bottom[x]);
		
		Z_ChangeTag (ds_source, PU_CACHE);
	}
}
