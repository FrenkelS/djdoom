//
// Copyright (C) 1993-1996 Id Software, Inc.
// Copyright (C) 2016-2017 Alexey Khokholov (Nuke.YKT)
// Copyright (C) 2017 Alexandre-Xavier Labonté-Lamoureux
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

// R_draw.c

#include <conio.h>
#include <dos.h>
#include "doomdef.h"
#include "r_local.h"

#define SC_INDEX			0x3c4
#define SC_MAPMASK			2
#define GC_INDEX			0x3ce
#define GC_READMAP			4
#define GC_MODE				5

/*

All drawing to the view buffer is accomplished in this file.  The other refresh
files only know about ccordinates, not the architecture of the frame buffer.

*/

int32_t viewwidth, scaledviewwidth, viewheight, viewwindowx, viewwindowy;

/*
==================
=
= R_DrawColumn
=
= Source is the top of the column to scale
=
==================
*/

lighttable_t	*dc_colormap	__attribute__ ((externally_visible));
int32_t			dc_x			__attribute__ ((externally_visible));
int32_t			dc_yl			__attribute__ ((externally_visible));
int32_t			dc_yh			__attribute__ ((externally_visible));
fixed_t			dc_iscale		__attribute__ ((externally_visible));
fixed_t			dc_texturemid	__attribute__ ((externally_visible));
byte			*dc_source		__attribute__ ((externally_visible));		// first pixel in a column (possibly virtual)

#if defined C_ONLY
void R_DrawColumn (void)
{
	int32_t		count;
	byte		*dest;
	fixed_t		frac, fracstep;	

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
				
#ifdef RANGECHECK
	if ((uint32_t)dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
		I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	outp (SC_INDEX+1,1<<(dc_x&3));
	dest = destview + dc_yl*PLANEWIDTH + (dc_x>>2);
	
	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

	do
	{
		*dest = dc_colormap[dc_source[(frac>>FRACBITS)&127]];
		dest += PLANEWIDTH;
		frac += fracstep;
	} while (count--);
}

void R_DrawColumnLow (void)
{
	int32_t		count;
	byte		*dest;
	fixed_t		frac, fracstep;	

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
				
#ifdef RANGECHECK
	if ((uint32_t)dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
		I_Error ("R_DrawColumnLow: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	if (dc_x & 1)
		outp (SC_INDEX+1,12);
	else
		outp (SC_INDEX+1,3);
	dest = destview + dc_yl*PLANEWIDTH + (dc_x>>1);
	
	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

	do
	{
		*dest = dc_colormap[dc_source[(frac>>FRACBITS)&127]];
		dest += PLANEWIDTH;
		frac += fracstep;
	} while (count--);
}
#endif


#define FUZZTABLE	50
#define FUZZOFF	(PLANEWIDTH)
static const int32_t		fuzzoffset[FUZZTABLE] = {
FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF
};
static int32_t fuzzpos = 0;

void R_DrawFuzzColumn (void)
{
	int32_t		count;
	byte		*dest;
	fixed_t		frac, fracstep;	

	if (!dc_yl)
		dc_yl = 1;
	if (dc_yh == viewheight-1)
		dc_yh = viewheight - 2;
		
	count = dc_yh - dc_yl;
	if (count < 0)
		return;
				
#ifdef RANGECHECK
	if ((uint32_t)dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
		I_Error ("R_DrawFuzzColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	if (detailshift)
	{
		if (dc_x & 1)
		{
			outpw (GC_INDEX,GC_READMAP+(2<<8) ); 
			outp (SC_INDEX+1,12); 
		}
		else
		{
			outpw (GC_INDEX,GC_READMAP); 
			outp (SC_INDEX+1,3); 
		}
		dest = destview + dc_yl*PLANEWIDTH + (dc_x>>1); 
	}
	else
	{
		outpw (GC_INDEX,GC_READMAP+((dc_x&3)<<8) ); 
		outp (SC_INDEX+1,1<<(dc_x&3)); 
		dest = destview + dc_yl*PLANEWIDTH + (dc_x>>2); 
	}

	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

	do
	{
		*dest = colormaps[6*256+dest[fuzzoffset[fuzzpos]]];
		if (++fuzzpos == FUZZTABLE)
			fuzzpos = 0;

		dest += PLANEWIDTH;
		frac += fracstep;
	} while (count--);
}

/*
========================
=
= R_DrawTranslatedColumn
=
========================
*/

byte *dc_translation;
byte *translationtables;

void R_DrawTranslatedColumn (void)
{
	int32_t		count;
	byte		*dest;
	fixed_t		frac, fracstep;	

	count = dc_yh - dc_yl;
	if (count < 0)
		return;
				
#ifdef RANGECHECK
	if ((uint32_t)dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
		I_Error ("R_DrawTranslatedColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	if (detailshift)
	{
		if (dc_x & 1)
			outp (SC_INDEX+1,12); 
		else
			outp (SC_INDEX+1,3);
	
		dest = destview + dc_yl*PLANEWIDTH + (dc_x>>1); 
	}
	else
	{
		outp (SC_INDEX+1,1<<(dc_x&3)); 
		dest = destview + dc_yl*PLANEWIDTH + (dc_x>>2); 
	}
	
	fracstep = dc_iscale;
	frac = dc_texturemid + (dc_yl-centery)*fracstep;

	do
	{
		*dest = dc_colormap[dc_translation[dc_source[frac>>FRACBITS]]];
		dest += PLANEWIDTH;
		frac += fracstep;
	} while (count--);
}

/*
========================
=
= R_InitTranslationTables
=
========================
*/

void R_InitTranslationTables (void)
{
	int32_t		i;

	translationtables = Z_Malloc (256*3+255, PU_STATIC, 0);
	translationtables = (byte *)(( (int32_t)translationtables + 255 )& ~255);

//
// translate just the 16 green colors
//
	for(i = 0; i < 256; i++)
	{
		if (i >= 0x70 && i<= 0x7f)
		{	// map green ramp to gray, brown, red
			translationtables[i] = 0x60 + (i&0xf);
			translationtables [i+256] = 0x40 + (i&0xf);
			translationtables [i+512] = 0x20 + (i&0xf);
		}
		else
		{
			translationtables[i] = translationtables[i+256] 
			= translationtables[i+512] = i;
		}
	}
}

/*
================
=
= R_DrawSpan
=
================
*/

int32_t			ds_y			__attribute__ ((externally_visible));
int32_t			ds_x1			__attribute__ ((externally_visible));
int32_t			ds_x2			__attribute__ ((externally_visible));
lighttable_t	*ds_colormap	__attribute__ ((externally_visible));
fixed_t			ds_xfrac		__attribute__ ((externally_visible));
fixed_t			ds_yfrac		__attribute__ ((externally_visible));
fixed_t			ds_xstep		__attribute__ ((externally_visible));
fixed_t			ds_ystep		__attribute__ ((externally_visible));
byte			*ds_source		__attribute__ ((externally_visible));		// start of a 64*64 tile image

#if defined C_ONLY
void R_DrawSpan (void) 
{ 
    fixed_t		xfrac;
    fixed_t		yfrac; 
    byte*		dest; 
    int32_t		spot; 
        int32_t                     i;
        int32_t                     prt;
        int32_t                     dsp_x1;
        int32_t                     dsp_x2;
        int32_t                     countp;
         
#ifdef RANGECHECK 
    if (ds_x2 < ds_x1
        || ds_x1<0
        || ds_x2>=SCREENWIDTH  
        || (uint32_t)ds_y>SCREENHEIGHT)
    {
        I_Error( "R_DrawSpan: %i to %i at %i",
                 ds_x1,ds_x2,ds_y);
    } 
#endif 

	for (i = 0; i < 4; i++)
	{
		dsp_x1 = (ds_x1-i)/4;
		if (dsp_x1*4+i<ds_x1)
			dsp_x1++;
		dsp_x2 = (ds_x2-i)/4;
		countp = dsp_x2 - dsp_x1;
		if (countp >= 0) {
			outp (SC_INDEX+1,1<<i); 
			dest = destview + ds_y*PLANEWIDTH + dsp_x1;

			prt = dsp_x1*4-ds_x1+i;
			xfrac = ds_xfrac+ds_xstep*prt;
			yfrac = ds_yfrac+ds_ystep*prt;

			do
			{
				// Current texture index in u,v.
				spot = ((yfrac>>(16-6))&(63*64)) + ((xfrac>>16)&63);

				// Lookup pixel from flat texture tile,
				//  re-index using light/colormap.
				*dest++ = ds_colormap[ds_source[spot]];
				// Next step in u,v.
				xfrac += ds_xstep*4; 
				yfrac += ds_ystep*4;
			} while (countp--);
		}
	}
} 

void R_DrawSpanLow (void) 
{ 
    fixed_t		xfrac;
    fixed_t		yfrac; 
    byte*		dest; 
    int32_t		spot; 
        int32_t                     prt;
        int32_t                     dsp_x1;
        int32_t                     dsp_x2;
        int32_t                     countp;
         
#ifdef RANGECHECK 
    if (ds_x2 < ds_x1
        || ds_x1<0
        || ds_x2>=SCREENWIDTH  
        || (uint32_t)ds_y>SCREENHEIGHT)
    {
        I_Error( "R_DrawSpanLow: %i to %i at %i",
                 ds_x1,ds_x2,ds_y);
    } 
#endif 

	dsp_x1 = (ds_x1)/2 + (ds_x1&1);
	dsp_x2 = ds_x2/2;
	countp = dsp_x2 - dsp_x1;
	if (countp >= 0) {
		outp (SC_INDEX+1,3); 
		dest = destview + ds_y*PLANEWIDTH + dsp_x1;

		prt = dsp_x1*2-ds_x1;
		xfrac = ds_xfrac+ds_xstep*prt; 
		yfrac = ds_yfrac+ds_ystep*prt;

		do
		{
			// Current texture index in u,v.
			spot = ((yfrac>>(16-6))&(63*64)) + ((xfrac>>16)&63);

			// Lookup pixel from flat texture tile,
			//  re-index using light/colormap.
			*dest++ = ds_colormap[ds_source[spot]];
			// Next step in u,v.
			xfrac += ds_xstep*2;
			yfrac += ds_ystep*2;
		} while (countp--);
	}

	dsp_x1 = (ds_x1-1)/2;
	if (dsp_x1*2+1<ds_x1)
		dsp_x1++;
	dsp_x2 = (ds_x2-1)/2;
	countp = dsp_x2 - dsp_x1;
	if (countp >= 0) {
		outp (SC_INDEX+1,12); 
		dest = destview + ds_y*PLANEWIDTH + dsp_x1;

		prt = dsp_x1*2-ds_x1+1;
		xfrac = ds_xfrac+ds_xstep*prt; 
		yfrac = ds_yfrac+ds_ystep*prt;

		do
		{
			// Current texture index in u,v.
			spot = ((yfrac>>(16-6))&(63*64)) + ((xfrac>>16)&63);

			// Lookup pixel from flat texture tile,
			//  re-index using light/colormap.
			*dest++ = ds_colormap[ds_source[spot]];
			// Next step in u,v.
			xfrac += ds_xstep*2;
			yfrac += ds_ystep*2;
		} while (countp--);
	}
}
#endif



/*
================
=
= R_InitBuffer
=
=================
*/

void R_InitBuffer (int32_t width, int32_t height)
{
	viewwindowx = (SCREENWIDTH-width) >> 1;

	if (width == SCREENWIDTH)
		viewwindowy = 0;
	else
		viewwindowy = (SCREENHEIGHT-SBARHEIGHT-height) >> 1;
}

 


/*
================
=
= R_FillBackScreen
=
= Fills the back screen with a pattern for variable screen sizes
= Also draws a beveled edge.
=================
*/

void R_FillBackScreen (void)
{
	byte		*src, *dest;
	int32_t		i, j;
	int32_t		x, y;
	patch_t		*patch;
	char		name1[] = "FLOOR7_2";
	char		name2[] = "GRNROCK";	
	char		*name;
	
	if (scaledviewwidth == SCREENWIDTH)
		return;
	
	if (commercial)
		name = name2;
	else
		name = name1;
    
	src = W_CacheLumpName (name, PU_CACHE);
	dest = screens[1];
	 
	for (y=0 ; y<SCREENHEIGHT-SBARHEIGHT ; y++)
	{
		for (x=0 ; x<SCREENWIDTH/64 ; x++)
		{
			memcpy (dest, src+((y&63)<<6), 64);
			dest += 64;
		}
	}
	
	patch = W_CacheLumpName ("brdr_t",PU_CACHE);
	for (x=0 ; x<scaledviewwidth ; x+=8)
		V_DrawPatch (viewwindowx+x,viewwindowy-8,1,patch);
	patch = W_CacheLumpName ("brdr_b",PU_CACHE);
	for (x=0 ; x<scaledviewwidth ; x+=8)
		V_DrawPatch (viewwindowx+x,viewwindowy+viewheight,1,patch);
	patch = W_CacheLumpName ("brdr_l",PU_CACHE);
	for (y=0 ; y<viewheight ; y+=8)
		V_DrawPatch (viewwindowx-8,viewwindowy+y,1,patch);
	patch = W_CacheLumpName ("brdr_r",PU_CACHE);
	for (y=0 ; y<viewheight ; y+=8)
		V_DrawPatch (viewwindowx+scaledviewwidth,viewwindowy+y,1,patch);

	V_DrawPatch (viewwindowx-8, viewwindowy-8, 1,
		W_CacheLumpName ("brdr_tl",PU_CACHE));
	V_DrawPatch (viewwindowx+scaledviewwidth, viewwindowy-8, 1,
		W_CacheLumpName ("brdr_tr",PU_CACHE));
	V_DrawPatch (viewwindowx-8, viewwindowy+viewheight, 1,
		W_CacheLumpName ("brdr_bl",PU_CACHE));
	V_DrawPatch (viewwindowx+scaledviewwidth, viewwindowy+viewheight, 1,
		W_CacheLumpName ("brdr_br",PU_CACHE));

	dest = (byte*)(0xac000 + __djgpp_conventional_base);
	src = screens[1];
	for (i = 0; i < 4; i++, src++)
	{
		outp (SC_INDEX, 2);
		outp (SC_INDEX+1, 1<<i);
		for (j = 0; j < (SCREENHEIGHT-SBARHEIGHT)*SCREENWIDTH/4; j++)
			dest[j] = src[j*4];
	}
}


void R_VideoErase (uint32_t ofs, int32_t count)
{ 
	int32_t		i;
	byte	*src, *dest;
	outp (SC_INDEX, SC_MAPMASK);
	outp (SC_INDEX+1, 15);
	outp (GC_INDEX, GC_MODE);
	outp (GC_INDEX+1, inp (GC_INDEX+1)|1);
	src = (byte*)(0xac000 + __djgpp_conventional_base + (ofs>>2));
	dest = destscreen+(ofs>>2);
	for (i = (count>>2)-1; i >= 0; i--)
	{
		dest[i] = src[i];
	}
	outp (GC_INDEX, GC_MODE);
	outp (GC_INDEX+1, inp (GC_INDEX+1)&~1);
}


/*
================
=
= R_DrawViewBorder
=
= Draws the border around the view for different size windows
=
=================
*/

void R_DrawViewBorder (void)
{ 
    int32_t		top, side, ofs, i;
 
	if (scaledviewwidth == SCREENWIDTH)
		return;
  
	top = ((SCREENHEIGHT-SBARHEIGHT)-viewheight)/2;
	side = (SCREENWIDTH-scaledviewwidth)/2;

//
// copy top and one line of left side
//
	R_VideoErase (0, top*SCREENWIDTH+side);
 
//
// copy one line of right side and bottom
//
	ofs = (viewheight+top)*SCREENWIDTH-side;
	R_VideoErase (ofs, top*SCREENWIDTH+side);
 
//
// copy sides using wraparound
//
	ofs = top*SCREENWIDTH + SCREENWIDTH-side;
	side <<= 1;
    
	for (i=1 ; i<viewheight ; i++)
	{
		R_VideoErase (ofs, side);
		ofs += SCREENWIDTH;
	}
}
 
 
