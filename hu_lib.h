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

#ifndef __HULIB__
#define __HULIB__


// foreground screen number
#define FG 0

// font stuff
#define HU_CHARERASE KEY_BACKSPACE

#define HU_MAXLINES 4
#define HU_MAXLINELENGTH 80

//
// Typedefs of widgets
//

// Text Line widget
//  (parent of Scrolling Text and Input Text widgets)
typedef struct
{
    // left-justified position of scrolling text window
    int32_t x, y;
    
    patch_t **f;			// font
    int32_t sc;			// start character
    char l[HU_MAXLINELENGTH+1];	// line of text
    int32_t len;		      	// current line length

    // whether this line needs to be udpated
    int32_t needsupdate;	      

} hu_textline_t;



// Scrolling Text window widget
//  (child of Text Line widget)
typedef struct
{
    hu_textline_t l[HU_MAXLINES];	// text lines to draw
    int32_t h;		// height in lines
    int32_t cl;		// current line number

    // pointer to boolean stating whether to update window
    boolean *on;
    boolean laston;		// last value of *->on.

} hu_stext_t;



// Input Text Line widget
//  (child of Text Line widget)
typedef struct
{
    hu_textline_t l;		// text line to input on

     // left margin past which I am not to delete characters
    int32_t lm;

    // pointer to boolean stating whether to update window
    boolean *on; 
    boolean laston; // last value of *->on;

} hu_itext_t;


//
// Widget creation, access, and update routines
//

//
// textline code
//

void	HUlib_initTextLine(hu_textline_t *t, int32_t x, int32_t y, patch_t **f, int32_t sc);

void	HUlib_addCharToTextLine(hu_textline_t *t, char ch);

// draws tline
void	HUlib_drawTextLine(hu_textline_t *l, boolean drawcursor);

// erases text line
void	HUlib_eraseTextLine(hu_textline_t *l); 


//
// Scrolling Text window widget routines
//

void	HUlib_initSText(hu_stext_t *s, int32_t x, int32_t y, int32_t h, patch_t **font, int32_t startchar, boolean *on);

void	HUlib_addMessageToSText (hu_stext_t *s, char *prefix, char *msg);

// draws stext
void	HUlib_drawSText(hu_stext_t *s);

// erases all stext lines
void	HUlib_eraseSText(hu_stext_t *s); 

// Input Text Line widget routines
void	HUlib_initIText(hu_itext_t *it, int32_t x, int32_t y, patch_t **font, int32_t startchar, boolean *on);

// resets line and left margin
void	HUlib_resetIText(hu_itext_t *it);

// whether eaten
boolean	HUlib_keyInIText(hu_itext_t *it, unsigned char ch);

void	HUlib_drawIText(hu_itext_t *it);

// erases all itext lines
void	HUlib_eraseIText(hu_itext_t *it); 

#endif
