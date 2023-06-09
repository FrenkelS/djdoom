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

// R_data.c

#include "doomdef.h"
#include "r_local.h"
#include "p_local.h"

typedef struct
{
	int32_t		originx;	// block origin (allways UL), which has allready
	int32_t		originy;	// accounted  for the patch's internal origin
	int32_t		patch;
} texpatch_t;

// a maptexturedef_t describes a rectangular texture, which is composed of one
// or more mappatch_t structures that arrange graphic patches
typedef struct
{
	char		name[8];		// for switch changing, etc
	int16_t		width;
	int16_t		height;
	int16_t		patchcount;
	texpatch_t	patches[1];		// [patchcount] drawn back to front
								//  into the cached texture
} texture_t;



int32_t		firstflat;
static int32_t		lastflat, numflats;
int32_t		firstspritelump, lastspritelump;
static int32_t	numspritelumps;

static int32_t			numtextures;
static texture_t	**textures;
static int32_t			*texturewidthmask;
fixed_t		*textureheight;		// needed for texture pegging
static int32_t			*texturecompositesize;
static int16_t		**texturecolumnlump;
static uint16_t		**texturecolumnofs;
static byte		**texturecomposite;

int32_t			*flattranslation;		// for global animation
int32_t			*texturetranslation;	// for global animation

fixed_t		*spritewidth;		// needed for pre rendering
fixed_t		*spriteoffset;
fixed_t		*spritetopoffset;

lighttable_t	*colormaps;


/*
==============================================================================

						MAPTEXTURE_T CACHING

when a texture is first needed, it counts the number of composite columns
required in the texture and allocates space for a column directory and any
new columns.  The directory will simply point inside other patches if there
is only one patch in a given column, but any columns with multiple patches
will have new column_ts generated.

==============================================================================
*/

/*
===================
=
= R_DrawColumnInCache
=
= Clip and draw a column from a patch into a cached post
=
===================
*/

static void R_DrawColumnInCache (column_t *patch, byte *cache, int32_t originy, int32_t cacheheight)
{
	int32_t		count, position;
	byte	*source;
	
	while (patch->topdelta != 0xff)
	{
		source = (byte *)patch + 3;
		count = patch->length;
		position = originy + patch->topdelta;
		if (position < 0)
		{
			count += position;
			position = 0;
		}
		if (position + count > cacheheight)
			count = cacheheight - position;
		if (count > 0)
			memcpy (cache + position, source, count);
		
		patch = (column_t *)(  (byte *)patch + patch->length + 4);
	}
}


/*
===================
=
= R_GenerateComposite
=
===================
*/

static void R_GenerateComposite (int32_t texnum)
{
	byte		*block;
	texture_t	*texture;
	texpatch_t	*patch;	
	patch_t		*realpatch;
	int32_t		x, x1, x2;
	int32_t		i;
	column_t	*patchcol;
	int16_t		*collump;
	uint16_t	*colofs;
	
	texture = textures[texnum];
	block = Z_Malloc (texturecompositesize[texnum], PU_STATIC, 
		&texturecomposite[texnum]);	
	collump = texturecolumnlump[texnum];
	colofs = texturecolumnofs[texnum];
		
//
// composite the columns together
//
	patch = texture->patches;
		
	for (i=0 , patch = texture->patches; i<texture->patchcount ; i++, patch++)
	{
		realpatch = W_CacheLumpNum (patch->patch, PU_CACHE);
		x1 = patch->originx;
		x2 = x1 + SHORT(realpatch->width);

		if (x1<0)
			x = 0;
		else
			x = x1;
		if (x2 > texture->width)
			x2 = texture->width;

		for ( ; x<x2 ; x++)
		{
			if (collump[x] >= 0)
				continue;		// column does not have multiple patches
			patchcol = (column_t *)((byte *)realpatch + 
				LONG(realpatch->columnofs[x-x1]));
			R_DrawColumnInCache (patchcol, block + colofs[x], patch->originy,
			texture->height);
		}
						
	}

// now that the texture has been built, it is purgable
	Z_ChangeTag (block, PU_CACHE);
}


/*
===================
=
= R_GenerateLookup
=
===================
*/

static void R_GenerateLookup (int32_t texnum)
{
	texture_t	*texture;
	byte		*patchcount;		// [texture->width]
	texpatch_t	*patch;	
	patch_t		*realpatch;
	int32_t		x, x1, x2;
	int32_t		i;
	int16_t		*collump;
	uint16_t	*colofs;
	
	texture = textures[texnum];

	texturecomposite[texnum] = 0;	// composited not created yet
	texturecompositesize[texnum] = 0;
	collump = texturecolumnlump[texnum];
	colofs = texturecolumnofs[texnum];
	
//
// count the number of columns that are covered by more than one patch
// fill in the lump / offset, so columns with only a single patch are
// all done
//
	patchcount = (byte *)alloca (texture->width);
	memset (patchcount, 0, texture->width);
	patch = texture->patches;
		
	for (i=0 , patch = texture->patches; i<texture->patchcount ; i++, patch++)
	{
		realpatch = W_CacheLumpNum (patch->patch, PU_CACHE);
		x1 = patch->originx;
		x2 = x1 + SHORT(realpatch->width);
		if (x1 < 0)
			x = 0;
		else
			x = x1;
		if (x2 > texture->width)
			x2 = texture->width;
		for ( ; x<x2 ; x++)
		{
			patchcount[x]++;
			collump[x] = patch->patch;
			colofs[x] = LONG(realpatch->columnofs[x-x1])+3;
		}
	}
	
	for (x=0 ; x<texture->width ; x++)
	{
		if (!patchcount[x])
		{
			printf ("R_GenerateLookup: column without a patch (%s)\n", texture->name);
			return;
		}
//			I_Error ("R_GenerateLookup: column without a patch");
		if (patchcount[x] > 1)
		{
			collump[x] = -1;	// use the cached block
			colofs[x] = texturecompositesize[texnum];
			if (texturecompositesize[texnum] > 0x10000-texture->height)
				I_Error ("R_GenerateLookup: texture %i is >64k",texnum);
			texturecompositesize[texnum] += texture->height;
		}
	}	
}


/*
================
=
= R_GetColumn
=
================
*/

byte *R_GetColumn (int32_t tex, int32_t col)
{
	int32_t	lump, ofs;
	
	col &= texturewidthmask[tex];
	lump = texturecolumnlump[tex][col];
	ofs = texturecolumnofs[tex][col];
	if (lump > 0)
		return (byte *)W_CacheLumpNum(lump,PU_CACHE)+ofs;
	if (!texturecomposite[tex])
		R_GenerateComposite (tex);
	return texturecomposite[tex] + ofs;
}


/*
==================
=
= R_InitTextures
=
= Initializes the texture list with the textures from the world map
=
==================
*/

static void R_InitTextures (void)
{
	maptexture_t	*mtexture;
	texture_t		*texture;
	mappatch_t		*mpatch;
	texpatch_t		*patch;
	int32_t			i,j;
	int32_t			*maptex, *maptex2, *maptex1;
	char			name[9], *names, *name_p;
	int32_t			*patchlookup;
	int32_t			totalwidth;
	int32_t			nummappatches;
	int32_t			offset, maxoff, maxoff2;
	int32_t			numtextures1, numtextures2;
	int32_t			*directory;
	int32_t			temp1, temp2, temp3;

//
// load the patch names from pnames.lmp
//
	name[8] = 0;
	names = W_CacheLumpName ("PNAMES", PU_STATIC);
	nummappatches = LONG ( *((int32_t *)names) );
	name_p = names+4;
	patchlookup = alloca (nummappatches*sizeof(*patchlookup));
	for (i=0 ; i<nummappatches ; i++)
	{
		strncpy (name,name_p+i*8, 8);
		patchlookup[i] = W_CheckNumForName (name);
	}
	Z_Free (names);

//
// load the map texture definitions from textures.lmp
//
	maptex = maptex1 = W_CacheLumpName ("TEXTURE1", PU_STATIC);
	numtextures1 = LONG(*maptex);
	maxoff = W_LumpLength (W_GetNumForName ("TEXTURE1"));
	directory = maptex+1;

	if (W_CheckNumForName ("TEXTURE2") != -1)
	{
		maptex2 = W_CacheLumpName ("TEXTURE2", PU_STATIC);
		numtextures2 = LONG(*maptex2);
		maxoff2 = W_LumpLength (W_GetNumForName ("TEXTURE2"));
	}
	else
	{
		maptex2 = NULL;
		numtextures2 = 0;
		maxoff2 = 0;
	}
	numtextures = numtextures1 + numtextures2;

	textures = Z_Malloc (numtextures*4, PU_STATIC, 0);
	texturecolumnlump = Z_Malloc (numtextures*4, PU_STATIC, 0);
	texturecolumnofs = Z_Malloc (numtextures*4, PU_STATIC, 0);
	texturecomposite = Z_Malloc (numtextures*4, PU_STATIC, 0);
	texturecompositesize = Z_Malloc (numtextures*4, PU_STATIC, 0);
	texturewidthmask = Z_Malloc (numtextures*4, PU_STATIC, 0);
	textureheight = Z_Malloc (numtextures*4, PU_STATIC, 0);

	totalwidth = 0;
    
	temp1 = W_GetNumForName ("S_START");
	temp2 = W_GetNumForName ("S_END") - 1;
	temp3 = ((temp2-temp1+63)/64) + ((numtextures+63)/64);
	printf("[");
	for (i = 0; i < temp3; i++)
		printf(" ");
	printf("         ]");
	for (i = 0; i < temp3; i++)
		printf("\x8");
	printf("\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8");	

	for (i=0 ; i<numtextures ; i++, directory++)
	{
		if (!(i&63))
			printf (".");
		if (i == numtextures1)
		{	// start looking in second texture file
			maptex = maptex2;
			maxoff = maxoff2;
			directory = maptex+1;
		}

		offset = LONG(*directory);
		if (offset > maxoff)
			I_Error ("R_InitTextures: bad texture directory");
		mtexture = (maptexture_t *) ( (byte *)maptex + offset);
		texture = textures[i] = Z_Malloc (sizeof(texture_t) 
			+ sizeof(texpatch_t)*(SHORT(mtexture->patchcount)-1), PU_STATIC,
			0);
		texture->width = SHORT(mtexture->width);
		texture->height = SHORT(mtexture->height);
		texture->patchcount = SHORT(mtexture->patchcount);
		memcpy (texture->name, mtexture->name, sizeof(texture->name));
		mpatch = &mtexture->patches[0];
		patch = &texture->patches[0];
		for (j=0 ; j<texture->patchcount ; j++, mpatch++, patch++)
		{
			patch->originx = SHORT(mpatch->originx);
			patch->originy = SHORT(mpatch->originy);
			patch->patch = patchlookup[SHORT(mpatch->patch)];
			if (patch->patch == -1)
				I_Error (
				"R_InitTextures: Missing patch in texture %s",texture->name);
		}		
		texturecolumnlump[i] = Z_Malloc (texture->width*2, PU_STATIC,0);
		texturecolumnofs[i] = Z_Malloc (texture->width*2, PU_STATIC,0);
		j = 1;
		while (j*2 <= texture->width)
			j<<=1;
		texturewidthmask[i] = j-1;
		textureheight[i] = texture->height<<FRACBITS;
		
		totalwidth += texture->width;
	}

	Z_Free (maptex1);
	if (maptex2)
		Z_Free (maptex2);

//
// precalculate whatever possible
//		
	for (i=0 ; i<numtextures ; i++)
		R_GenerateLookup(i);

//
// translation table for global animation
//
	texturetranslation = Z_Malloc ((numtextures+1)*4, PU_STATIC, 0);
	for (i=0 ; i<numtextures ; i++)
		texturetranslation[i] = i;
}


/*
================
=
= R_InitFlats
=
=================
*/

static void R_InitFlats (void)
{
	int32_t		i;
	
	firstflat = W_GetNumForName ("F_START") + 1;
	lastflat = W_GetNumForName ("F_END") - 1;
	numflats = lastflat - firstflat + 1;
	
// translation table for global animation
	flattranslation = Z_Malloc ((numflats+1)*4, PU_STATIC, 0);
	for (i=0 ; i<numflats ; i++)
		flattranslation[i] = i;
}


/*
================
=
= R_InitSpriteLumps
=
= Finds the width and hoffset of all sprites in the wad, so the sprite doesn't
= need to be cached just for the header during rendering
=================
*/

static void R_InitSpriteLumps (void)
{
	int32_t		i;
	patch_t	*patch;

	firstspritelump = W_GetNumForName ("S_START") + 1;
	lastspritelump = W_GetNumForName ("S_END") - 1;
	numspritelumps = lastspritelump - firstspritelump + 1;
	spritewidth = Z_Malloc (numspritelumps*4, PU_STATIC, 0);
	spriteoffset = Z_Malloc (numspritelumps*4, PU_STATIC, 0);
	spritetopoffset = Z_Malloc (numspritelumps*4, PU_STATIC, 0);

	for (i=0 ; i< numspritelumps ; i++)
	{
		if (!(i&63))
			printf (".");
		patch = W_CacheLumpNum (firstspritelump+i, PU_CACHE);
		spritewidth[i] = SHORT(patch->width)<<FRACBITS;
		spriteoffset[i] = SHORT(patch->leftoffset)<<FRACBITS;
		spritetopoffset[i] = SHORT(patch->topoffset)<<FRACBITS;
	}
}


/*
================
=
= R_InitColormaps
=
=================
*/

static void R_InitColormaps (void)
{
	int32_t	lump, length;
//
// load in the light tables
// 256 byte align tables
//
	lump = W_GetNumForName("COLORMAP");
	length = W_LumpLength (lump) + 255;
	colormaps = Z_Malloc (length, PU_STATIC, 0);
	colormaps = (byte *)( ((int32_t)colormaps + 255)&~0xff);
	W_ReadLump (lump,colormaps);
}


/*
================
=
= R_InitData
=
= Locates all the lumps that will be used by all views
= Must be called after W_Init
=================
*/

void R_InitData (void)
{
	R_InitTextures ();
	printf (".");
	R_InitFlats ();
	printf (".");
	R_InitSpriteLumps ();
	printf (".");
	R_InitColormaps ();
}


//=============================================================================

/*
================
=
= R_FlatNumForName
=
================
*/

int32_t	R_FlatNumForName (char *name)
{
	int32_t	i = W_GetNumForName (name);
	return i - firstflat;
}


/*
================
=
= R_CheckTextureNumForName
=
================
*/

int32_t	R_CheckTextureNumForName (char *name)
{
	int32_t		i;
	
	if (name[0] == '-')		// no texture marker
		return 0;
		
	for (i=0 ; i<numtextures ; i++)
		if (!strnicmp (textures[i]->name, name, 8) )
			return i;
		
	return -1;
}


/*
================
=
= R_TextureNumForName
=
================
*/

int32_t	R_TextureNumForName (char *name)
{
	int32_t	i = R_CheckTextureNumForName (name);
	if (i==-1)
		I_Error ("R_TextureNumForName: %s not found",name);
	
	return i;
}


/*
=================
=
= R_PrecacheLevel
=
= Preloads all relevent graphics for the level
=================
*/

static int32_t		flatmemory, texturememory, spritememory;

void R_PrecacheLevel (void)
{
	char			*flatpresent;
	char			*texturepresent;
	char			*spritepresent;
	int32_t			i,j,k, lump;
	texture_t		*texture;
	thinker_t		*th;
	spriteframe_t	*sf;

	if (demoplayback)
		return;
			
//
// precache flats
//	
	flatpresent = alloca(numflats);
	memset (flatpresent,0,numflats);	
	for (i=0 ; i<numsectors ; i++)
	{
		flatpresent[sectors[i].floorpic] = 1;
		flatpresent[sectors[i].ceilingpic] = 1;
	}
	
	flatmemory = 0;
	for (i=0 ; i<numflats ; i++)
		if (flatpresent[i])
		{
			lump = firstflat + i;
			flatmemory += lumpinfo[lump].size;
			W_CacheLumpNum(lump, PU_CACHE);
		}
		
//
// precache textures
//
	texturepresent = alloca(numtextures);
	memset (texturepresent,0, numtextures);
	
	for (i=0 ; i<numsides ; i++)
	{
		texturepresent[sides[i].toptexture] = 1;
		texturepresent[sides[i].midtexture] = 1;
		texturepresent[sides[i].bottomtexture] = 1;
	}
	
	texturepresent[skytexture] = 1;
	
	texturememory = 0;
	for (i=0 ; i<numtextures ; i++)
	{
		if (!texturepresent[i])
			continue;
		texture = textures[i];
		for (j=0 ; j<texture->patchcount ; j++)
		{
			lump = texture->patches[j].patch;
			texturememory += lumpinfo[lump].size;
			W_CacheLumpNum(lump , PU_CACHE);
		}
	}
	
//
// precache sprites
//
	spritepresent = alloca(NUMSPRITES);
	memset (spritepresent,0, NUMSPRITES);
	
	for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
	{
		if (th->function == P_MobjThinker)
			spritepresent[((mobj_t *)th)->sprite] = 1;
	}
	
	spritememory = 0;
	for (i=0 ; i<NUMSPRITES ; i++)
	{
		if (!spritepresent[i])
			continue;
		for (j=0 ; j<sprites[i].numframes ; j++)
		{
			sf = &sprites[i].spriteframes[j];
			for (k=0 ; k<8 ; k++)
			{
				lump = firstspritelump + sf->lump[k];
				spritememory += lumpinfo[lump].size;
				W_CacheLumpNum(lump , PU_CACHE);
			}
		}
	}
}




