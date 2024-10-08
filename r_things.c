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

// R_things.c
#include "doomdef.h"
#include "r_local.h"

typedef struct
{
	int32_t		x1, x2;

	int32_t		column;
	int32_t		topclip;
	int32_t		bottomclip;
} maskdraw_t;

/*

Sprite rotation 0 is facing the viewer, rotation 1 is one angle turn CLOCKWISE around the axis.
This is not the same as the angle, which increases counter clockwise
(protractor).  There was a lot of stuff grabbed wrong, so I changed it...

*/


fixed_t		pspritescale, pspriteiscale;

static lighttable_t	**spritelights;

// constant arrays used for psprite clipping and initializing clipping
int16_t	negonearray[SCREENWIDTH];
int16_t	screenheightarray[SCREENWIDTH];

/*
===============================================================================

						INITIALIZATION FUNCTIONS

===============================================================================
*/

// variables used to look up and range check thing_t sprites patches
spritedef_t		*sprites;

static spriteframe_t	sprtemp[29];
static int32_t			maxframe;
static char				*spritename;



/*
=================
=
= R_InstallSpriteLump
=
= Local function for R_InitSprites
=================
*/

static void R_InstallSpriteLump (int32_t lump, uint32_t frame, uint32_t rotation, boolean flipped)
{
	int32_t		r;

	if (frame >= 29 || rotation > 8)
		I_Error ("R_InstallSpriteLump: Bad frame characters in lump %i", lump);

	if ((int32_t)frame > maxframe)
		maxframe = frame;

	if (rotation == 0)
	{
// the lump should be used for all rotations
		if (sprtemp[frame].rotate == false)
			I_Error ("R_InstallSpriteLump: Sprite %s frame %c has multip rot=0 lump"
			, spritename, 'A'+frame);
		if (sprtemp[frame].rotate == true)
			I_Error ("R_InstallSpriteLump: Sprite %s frame %c has rotations and a rot=0 lump"
			, spritename, 'A'+frame);

		sprtemp[frame].rotate = false;
		for (r=0 ; r<8 ; r++)
		{
			sprtemp[frame].lump[r] = lump - firstspritelump;
			sprtemp[frame].flip[r] = (byte)flipped;
		}
		return;
	}

// the lump is only used for one rotation
	if (sprtemp[frame].rotate == false)
		I_Error ("R_InstallSpriteLump: Sprite %s frame %c has rotations and a rot=0 lump"
		, spritename, 'A'+frame);

	sprtemp[frame].rotate = true;

	rotation--;		// make 0 based
	if (sprtemp[frame].lump[rotation] != -1)
		I_Error ("R_InstallSpriteLump: Sprite %s : %c : %c has two lumps mapped to it"
		,spritename, 'A'+frame, '1'+rotation);

	sprtemp[frame].lump[rotation] = lump - firstspritelump;
	sprtemp[frame].flip[rotation] = (byte)flipped;
}

/*
=================
=
= R_InitSpriteDefs
=
= Pass a null terminated list of sprite names (4 chars exactly) to be used
= Builds the sprite rotation matrixes to account for horizontally flipped
= sprites.  Will report an error if the lumps are inconsistant
=Only called at startup
=
= Sprite lump names are 4 characters for the actor, a letter for the frame,
= and a number for the rotation, A sprite that is flippable will have an
= additional letter/number appended.  The rotation character can be 0 to
= signify no rotations
=================
*/

static void R_InitSpriteDefs (void)
{
	int32_t		i, l, intname, frame, rotation;
	int32_t		start, end;
	int32_t		patched;

	sprites = Z_Malloc(NUMSPRITES *sizeof(*sprites), PU_STATIC, NULL);

	start = firstspritelump-1;
	end = lastspritelump+1;

// scan all the lump names for each of the names, noting the highest
// frame letter
// Just compare 4 characters as ints
	for (i=0 ; i<NUMSPRITES ; i++)
	{
		spritename = sprnames[i];
		memset (sprtemp,-1, sizeof(sprtemp));

		maxframe = -1;
		intname = *(int32_t *)sprnames[i];

		//
		// scan the lumps, filling in the frames for whatever is found
		//
		for (l=start+1 ; l<end ; l++)
			if (*(int32_t *)lumpinfo[l].name == intname)
			{
				frame = lumpinfo[l].name[4] - 'A';
				rotation = lumpinfo[l].name[5] - '0';

				if (modifiedgame)
					patched = W_GetNumForName (lumpinfo[l].name);
				else
					patched = l;

				R_InstallSpriteLump (patched, frame, rotation, false);
				if (lumpinfo[l].name[6])
				{
					frame = lumpinfo[l].name[6] - 'A';
					rotation = lumpinfo[l].name[7] - '0';
					R_InstallSpriteLump (l, frame, rotation, true);
				}
			}

		//
		// check the frames that were found for completeness
		//
		if (maxframe == -1)
		{
			sprites[i].numframes = 0;
			continue;
		}

		maxframe++;
		for (frame = 0 ; frame < maxframe ; frame++)
		{
			switch ((int32_t)sprtemp[frame].rotate)
			{
			case -1:	// no rotations were found for that frame at all
				I_Error ("R_InitSpriteDefs: No patches found for %s frame %c"
				, sprnames[i], frame+'A');
			case 0:	// only the first rotation is needed
				break;

			case 1:	// must have all 8 frames
				for (rotation=0 ; rotation<8 ; rotation++)
					if (sprtemp[frame].lump[rotation] == -1)
						I_Error ("R_InitSpriteDefs: Sprite %s frame %c is missing rotations"
						, sprnames[i], frame+'A');
			}
		}

		//
		// allocate space for the frames present and copy sprtemp to it
		//
		sprites[i].numframes = maxframe;
		sprites[i].spriteframes =
			Z_Malloc (maxframe * sizeof(spriteframe_t), PU_STATIC, NULL);
		memcpy (sprites[i].spriteframes, sprtemp, maxframe*sizeof(spriteframe_t));
	}

}


/*
===============================================================================

							GAME FUNCTIONS

===============================================================================
*/
#define	MAXVISSPRITES	128

static int32_t		num_vissprite;
static vissprite_t	vissprites[MAXVISSPRITES];
static vissprite_t*	vissprite_ptrs[MAXVISSPRITES];


/*
===================
=
= R_InitSprites
=
= Called at program start
===================
*/

void R_InitSprites (void)
{
	int32_t		i;

	for (i=0 ; i<SCREENWIDTH ; i++)
	{
		negonearray[i] = -1;
	}

	R_InitSpriteDefs ();
}


/*
===================
=
= R_ClearSprites
=
= Called at frame start
===================
*/

void R_ClearSprites (void)
{
	num_vissprite = 0;
}


/*
===================
=
= R_NewVisSprite
=
===================
*/

static vissprite_t		overflowsprite;

static vissprite_t *R_NewVisSprite (void)
{
	if (num_vissprite >= MAXVISSPRITES)
		return &overflowsprite;

	return vissprites + num_vissprite++;
}


/*
================
=
= R_DrawMaskedColumn
=
= Used for sprites and masked mid textures
================
*/

int16_t		*mfloorclip;
int16_t		*mceilingclip;
fixed_t		spryscale;
fixed_t		sprtopscreen;

void R_DrawMaskedColumn (column_t *column)
{
	int32_t		topscreen, bottomscreen;
	fixed_t	basetexturemid;

	basetexturemid = dc_texturemid;

	for ( ; column->topdelta != 0xff ; )
	{
// calculate unclipped screen coordinates for post
		topscreen = sprtopscreen + spryscale*column->topdelta;
		bottomscreen = topscreen + spryscale*column->length;
		dc_yl = (topscreen+FRACUNIT-1)>>FRACBITS;
		dc_yh = (bottomscreen-1)>>FRACBITS;

		if (dc_yh >= mfloorclip[dc_x])
			dc_yh = mfloorclip[dc_x]-1;
		if (dc_yl <= mceilingclip[dc_x])
			dc_yl = mceilingclip[dc_x]+1;

		if (dc_yl <= dc_yh)
		{
			dc_source = (byte *)column + 3;
			dc_texturemid = basetexturemid - (column->topdelta<<FRACBITS);
//			dc_source = (byte *)column + 3 - column->topdelta;
			colfunc ();		// either R_DrawColumn or R_DrawFuzzColumn
		}
		column = (column_t *)(  (byte *)column + column->length + 4);
	}

	dc_texturemid = basetexturemid;
}


/*
================
=
= R_DrawVisSprite
=
= mfloorclip and mceilingclip should also be set
================
*/

static void R_DrawVisSprite (vissprite_t *vis)
{
	column_t	*column;
	int32_t		texturecolumn;
	fixed_t		frac;
	patch_t		*patch;


	patch = W_CacheLumpNum(vis->patch+firstspritelump, PU_CACHE);

	dc_colormap = vis->colormap;

	if (!dc_colormap)
		colfunc = fuzzcolfunc;		// NULL colormap = shadow draw
	else if (vis->mobjflags & MF_TRANSLATION)
	{
		colfunc = R_DrawTranslatedColumn;
		dc_translation = translationtables - 256 +
			( (vis->mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT-8) );
	}

	dc_iscale = abs(vis->xiscale)>>detailshift;
	dc_texturemid = vis->texturemid;
	frac = vis->startfrac;
	spryscale = vis->scale;

	sprtopscreen = centeryfrac - FixedMul(dc_texturemid,spryscale);

	for (dc_x=vis->x1 ; dc_x<=vis->x2 ; dc_x++, frac += vis->xiscale)
	{
		texturecolumn = frac>>FRACBITS;
#ifdef RANGECHECK
		if (texturecolumn < 0 || texturecolumn >= SHORT(patch->width))
			I_Error ("R_DrawSpriteRange: bad texturecolumn");
#endif
		column = (column_t *) ((byte *)patch +
		 LONG(patch->columnofs[texturecolumn]));
		 R_DrawMaskedColumn (column);
	}

	colfunc = basecolfunc;
}



/*
===================
=
= R_ProjectSprite
=
= Generates a vissprite for a thing if it might be visible
=
===================
*/

static void R_ProjectSprite (mobj_t *thing)
{
	fixed_t		trx,try;
	fixed_t		gxt,gyt;
	fixed_t		tx,tz;
	fixed_t		xscale;
	int32_t		x1, x2;
	spritedef_t	*sprdef;
	spriteframe_t	*sprframe;
	int32_t		lump;
	uint32_t	rot;
	boolean		flip;
	int32_t		index;
	vissprite_t	*vis;
	angle_t		ang;
	fixed_t		iscale;

//
// transform the origin point
//
	trx = thing->x - viewx;
	try = thing->y - viewy;

	gxt = FixedMul(trx,viewcos);
	gyt = -FixedMul(try,viewsin);
	tz = gxt-gyt;

	if (tz < MINZ)
		return;		// thing is behind view plane
	xscale = FixedDiv(projection, tz);

	gxt = -FixedMul(trx,viewsin);
	gyt = FixedMul(try,viewcos);
	tx = -(gyt+gxt);
	
	if (abs(tx)>(tz<<2))
		return;		// too far off the side

//
// decide which patch to use for sprite reletive to player
//
#ifdef RANGECHECK
	if ((uint32_t)thing->sprite >= NUMSPRITES)
		I_Error ("R_ProjectSprite: invalid sprite number %i ",thing->sprite);
#endif
	sprdef = &sprites[thing->sprite];
#ifdef RANGECHECK
	if ( (thing->frame&FF_FRAMEMASK) >= sprdef->numframes )
		I_Error ("R_ProjectSprite: invalid sprite frame %i : %i "
		,thing->sprite, thing->frame);
#endif
	sprframe = &sprdef->spriteframes[ thing->frame & FF_FRAMEMASK];

	if (sprframe->rotate)
	{	// choose a different rotation based on player view
		ang = R_PointToAngle (thing->x, thing->y);
		rot = (ang-thing->angle+(uint32_t)(ANG45/2)*9)>>29;
		lump = sprframe->lump[rot];
		flip = (boolean)sprframe->flip[rot];
	}
	else
	{	// use single rotation for all views
		lump = sprframe->lump[0];
		flip = (boolean)sprframe->flip[0];
	}

//
// calculate edges of the shape
//
	tx -= spriteoffset[lump];
	x1 = (centerxfrac + FixedMul (tx,xscale) ) >>FRACBITS;
	if (x1 > viewwidth)
		return;		// off the right side
	tx +=  spritewidth[lump];
	x2 = ((centerxfrac + FixedMul (tx,xscale) ) >>FRACBITS) - 1;
	if (x2 < 0)
		return;		// off the left side


//
// store information in a vissprite
//
	vis = R_NewVisSprite ();
	vis->mobjflags = thing->flags;
	vis->scale = xscale<<detailshift;
	vis->gx = thing->x;
	vis->gy = thing->y;
	vis->gz = thing->z;
	vis->gzt = thing->z + spritetopoffset[lump];
	vis->texturemid = vis->gzt - viewz;

	vis->x1 = x1 < 0 ? 0 : x1;
	vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;
	iscale = FixedDiv (FRACUNIT, xscale);
	if (flip)
	{
		vis->startfrac = spritewidth[lump]-1;
		vis->xiscale = -iscale;
	}
	else
	{
		vis->startfrac = 0;
		vis->xiscale = iscale;
	}
	if (vis->x1 > x1)
		vis->startfrac += vis->xiscale*(vis->x1-x1);
	vis->patch = lump;
//
// get light level
//

	if (thing->flags & MF_SHADOW)
		vis->colormap = NULL;			// shadow draw
	else if (fixedcolormap)
		vis->colormap = fixedcolormap;	// fixed map
	else if (thing->frame & FF_FULLBRIGHT)
		vis->colormap = colormaps;		// full bright
	else
	{									// diminished light
		index = xscale>>(LIGHTSCALESHIFT-detailshift);
		if (index >= MAXLIGHTSCALE)
			index = MAXLIGHTSCALE-1;
		vis->colormap = spritelights[index];
	}
}




/*
========================
=
= R_AddSprites
=
========================
*/

void R_AddSprites (sector_t *sec)
{
	mobj_t		*thing;
	int32_t		lightnum;

	if (sec->validcount == validcount)
		return;		// already added

	sec->validcount = validcount;

	lightnum = (sec->lightlevel >> LIGHTSEGSHIFT)+extralight;
	if (lightnum < 0)
		spritelights = scalelight[0];
	else if (lightnum >= LIGHTLEVELS)
		spritelights = scalelight[LIGHTLEVELS-1];
	else
		spritelights = scalelight[lightnum];


	for (thing = sec->thinglist ; thing ; thing = thing->snext)
		R_ProjectSprite (thing);
}


/*
========================
=
= R_DrawPSprite
=
========================
*/

static void R_DrawPSprite (pspdef_t *psp)
{
	fixed_t		tx;
	int32_t		x1, x2;
	spritedef_t	*sprdef;
	spriteframe_t	*sprframe;
	int32_t		lump;
	boolean		flip;
	vissprite_t	*vis, avis;

//
// decide which patch to use
//
#ifdef RANGECHECK
	if ( (uint32_t)psp->state->sprite >= NUMSPRITES)
		I_Error ("R_ProjectSprite: invalid sprite number %i "
		, psp->state->sprite);
#endif
	sprdef = &sprites[psp->state->sprite];
#ifdef RANGECHECK
	if ( (psp->state->frame & FF_FRAMEMASK)  >= sprdef->numframes)
		I_Error ("R_ProjectSprite: invalid sprite frame %i : %i "
		, psp->state->sprite, psp->state->frame);
#endif
	sprframe = &sprdef->spriteframes[ psp->state->frame & FF_FRAMEMASK ];

	lump = sprframe->lump[0];
	flip = (boolean)sprframe->flip[0];

//
// calculate edges of the shape
//
	tx = psp->sx-160*FRACUNIT;

	tx -= spriteoffset[lump];
	x1 = (centerxfrac + FixedMul (tx,pspritescale) ) >>FRACBITS;
	if (x1 > viewwidth)
		return;		// off the right side
	tx +=  spritewidth[lump];
	x2 = ((centerxfrac + FixedMul (tx, pspritescale) ) >>FRACBITS) - 1;
	if (x2 < 0)
		return;		// off the left side

//
// store information in a vissprite
//
	vis = &avis;
	vis->mobjflags = 0;
	vis->texturemid = (BASEYCENTER<<FRACBITS)+FRACUNIT/2-(psp->sy-spritetopoffset[lump]);
	vis->x1 = x1 < 0 ? 0 : x1;
	vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;
	vis->scale = pspritescale<<detailshift;
	if (flip)
	{
		vis->xiscale = -pspriteiscale;
		vis->startfrac = spritewidth[lump]-1;
	}
	else
	{
		vis->xiscale = pspriteiscale;
		vis->startfrac = 0;
	}
	if (vis->x1 > x1)
		vis->startfrac += vis->xiscale*(vis->x1-x1);
	vis->patch = lump;

	if (viewplayer->powers[pw_invisibility] > 4*32 ||
	viewplayer->powers[pw_invisibility] & 8)
		vis->colormap = NULL;			// shadow draw
	else if (fixedcolormap)
		vis->colormap = fixedcolormap;	// fixed color
	else if (psp->state->frame & FF_FULLBRIGHT)
		vis->colormap = colormaps;		// full bright
	else
		vis->colormap = spritelights[MAXLIGHTSCALE-1];	// local light
	R_DrawVisSprite (vis);
}

/*
========================
=
= R_DrawPlayerSprites
=
========================
*/

static void R_DrawPlayerSprites (void)
{
	int32_t		i, lightnum;
	pspdef_t	*psp;

//
// get light level
//
	lightnum = (viewplayer->mo->subsector->sector->lightlevel >> LIGHTSEGSHIFT)
		+extralight;
	if (lightnum < 0)
		spritelights = scalelight[0];
	else if (lightnum >= LIGHTLEVELS)
		spritelights = scalelight[LIGHTLEVELS-1];
	else
		spritelights = scalelight[lightnum];
//
// clip to screen bounds
//
	mfloorclip = screenheightarray;
	mceilingclip = negonearray;

//
// add all active psprites
//
	for (i=0, psp=viewplayer->psprites ; i<NUMPSPRITES ; i++,psp++)
		if (psp->state)
			R_DrawPSprite (psp);

}


/*
========================
=
= R_SortVisSprites
=
========================
*/

#define bcopyp(d, s, n) memcpy(d, s, (n) * sizeof(void *))

// merge sort
static void msort(vissprite_t **s, vissprite_t **t, int32_t n)
{
	if (n >= 16)
	{
		int32_t n1 = n / 2, n2 = n - n1;
		vissprite_t **s1 = s, **s2 = s + n1, **d = t;

		msort(s1, t, n1);
		msort(s2, t, n2);

		while ((*s1)->scale > (*s2)->scale ? (*d++ = *s1++, --n1) : (*d++ = *s2++, --n2))
			;

		if (n2)
			bcopyp(d, s2, n2);
		else
			bcopyp(d, s1, n1);

		bcopyp(s, t, n);
	}
	else
	{
		for (int32_t i = 1; i < n; i++)
		{
			vissprite_t *temp = s[i];
			if (s[i - 1]->scale < temp->scale)
			{
				int32_t j = i;
				while ((s[j] = s[j - 1])->scale < temp->scale && --j)
					;
				s[j] = temp;
			}
		}
	}
}

static void R_SortVisSprites (void)
{
	int32_t i = num_vissprite;

	if (i)
	{
		while (--i >= 0)
			vissprite_ptrs[i] = vissprites + i;

		msort(vissprite_ptrs, vissprite_ptrs + num_vissprite, num_vissprite);
	}
}



/*
========================
=
= R_DrawSprite
=
========================
*/

static void R_DrawSprite (vissprite_t *spr)
{
	drawseg_t		*ds;
	int16_t			clipbot[SCREENWIDTH], cliptop[SCREENWIDTH];
	int32_t			x, r1, r2;
	fixed_t			scale, lowscale;
	int32_t			silhouette;

	for (x = spr->x1 ; x<=spr->x2 ; x++)
		clipbot[x] = cliptop[x] = -2;

//
// scan drawsegs from end to start for obscuring segs
// the first drawseg that has a greater scale is the clip seg
//
	for (ds=ds_p-1 ; ds >= drawsegs ; ds--)
	{
		//
		// determine if the drawseg obscures the sprite
		//
		if (ds->x1 > spr->x2 || ds->x2 < spr->x1 ||
		(!ds->silhouette && !ds->maskedtexturecol) )
			continue;			// doesn't cover sprite

		r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
		r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;
		if (ds->scale1 > ds->scale2)
		{
			lowscale = ds->scale2;
			scale = ds->scale1;
		}
		else
		{
			lowscale = ds->scale1;
			scale = ds->scale2;
		}

		if (scale < spr->scale || ( lowscale < spr->scale
		&& !R_PointOnSegSide (spr->gx, spr->gy, ds->curline) ) )
		{
			if (ds->maskedtexturecol)	// masked mid texture
				R_RenderMaskedSegRange (ds, r1, r2);
			continue;			// seg is behind sprite
		}

//
// clip this piece of the sprite
//
		silhouette = ds->silhouette;
		if (spr->gz >= ds->bsilheight)
			silhouette &= ~SIL_BOTTOM;
		if (spr->gzt <= ds->tsilheight)
			silhouette &= ~SIL_TOP;

		if (silhouette == 1)
		{	// bottom sil
			for (x=r1 ; x<=r2 ; x++)
				if (clipbot[x] == -2)
					clipbot[x] = ds->sprbottomclip[x];
		}
		else if (silhouette == 2)
		{	// top sil
			for (x=r1 ; x<=r2 ; x++)
				if (cliptop[x] == -2)
					cliptop[x] = ds->sprtopclip[x];
		}
		else if (silhouette == 3)
		{	// both
			for (x=r1 ; x<=r2 ; x++)
			{
				if (clipbot[x] == -2)
					clipbot[x] = ds->sprbottomclip[x];
				if (cliptop[x] == -2)
					cliptop[x] = ds->sprtopclip[x];
			}
		}

	}

//
// all clipping has been performed, so draw the sprite
//

// check for unclipped columns
	for (x = spr->x1 ; x<=spr->x2 ; x++)
	{
		if (clipbot[x] == -2)
			clipbot[x] = viewheight;
		if (cliptop[x] == -2)
			cliptop[x] = -1;
	}

	mfloorclip = clipbot;
	mceilingclip = cliptop;
	R_DrawVisSprite (spr);
}


/*
========================
=
= R_DrawMasked
=
========================
*/

void R_DrawMasked (void)
{
	drawseg_t		*ds;

	R_SortVisSprites ();

	// draw all vissprites back to front
	for (int32_t i = num_vissprite; --i >= 0; )
		R_DrawSprite(vissprite_ptrs[i]);

//
// render any remaining masked mid textures
//
	for (ds=ds_p-1 ; ds >= drawsegs ; ds--)
		if (ds->maskedtexturecol)
			R_RenderMaskedSegRange (ds, ds->x1, ds->x2);

//
// draw the psprites on top of everything
//
	if (!viewangleoffset)		// don't draw on side views
		R_DrawPlayerSprites ();
}


