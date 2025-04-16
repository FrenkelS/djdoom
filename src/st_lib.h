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

#ifndef __STLIB__
#define __STLIB__

//
// Background and foreground screen numbers
//
#define BG 4
#define FG 0

//
// Typedefs of widgets
//

// Number widget

typedef struct
{
  // upper right-hand corner
  //  of the number (right-justified)
  int32_t x, y;

  // max # of digits in number
  int32_t width;    

  // last number value
  int32_t oldnum;
    
  // pointer to current value
  int32_t *num;

  // pointer to boolean stating
  //  whether to update number
  boolean *on;

  // list of patches for 0-9
  patch_t **p;

  // user data
  int32_t data;
    
} st_number_t;



// Percent widget ("child" of number widget,
//  or, more precisely, contains a number widget.)
typedef struct
{
  // number information
  st_number_t n;

  // percent sign graphic
  patch_t *p;
    
} st_percent_t;



// Multiple Icon widget
typedef struct
{
  // center-justified location of icons
  int32_t x, y;

  // last icon number
  int32_t oldinum;

  // pointer to current icon
  int32_t *inum;

  // pointer to boolean stating
  //  whether to update icon
  boolean *on;

  // list of icons
  patch_t **p;
    
  // user data
  int32_t data;
    
} st_multicon_t;




// Binary Icon widget

typedef struct
{
  // center-justified location of icon
  int32_t x, y;

  // last icon value
  int32_t oldval;

  // pointer to current icon status
  boolean *val;

  // pointer to boolean
  //  stating whether to update icon
  boolean *on;  


  patch_t *p;	// icon
  int32_t data;   // user data
    
} st_binicon_t;



//
// Widget creation, access, and update routines
//

// Initializes widget library.
// More precisely, initialize STMINUS,
//  everything else is done somewhere else.
//
void STlib_init(void);



// Number widget routines
void STlib_initNum (st_number_t *n, int32_t x, int32_t y, patch_t **pl, int32_t *num, boolean *on, int32_t width);

void STlib_updateNum (st_number_t *n);


// Percent widget routines
void STlib_initPercent (st_percent_t *p, int32_t x, int32_t y, patch_t **pl, int32_t *num, boolean *on, patch_t *percent);

void STlib_updatePercent (st_percent_t *per, boolean refresh);


// Multiple Icon widget routines
void STlib_initMultIcon (st_multicon_t *mi, int32_t x, int32_t y, patch_t **il, int32_t *inum, boolean *on);

void STlib_updateMultIcon (st_multicon_t *mi, boolean refresh);

// Binary Icon widget routines
void STlib_initBinIcon (st_binicon_t *b, int32_t x, int32_t y, patch_t *i, boolean *val, boolean *on);

void STlib_updateBinIcon (st_binicon_t *bi, boolean refresh);

#endif
