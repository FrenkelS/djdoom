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

// AM_data.h : The vector graphics for the automap

#ifndef __AMDATA_H__
#define __AMDATA_H__

#pragma once

// a line drawing of the player pointing right, starting from the middle.

#define R ((8*PLAYERRADIUS)/7)

static mline_t player_arrow[] = {
  { { -R+R/8, 0 }, { R, 0 } }, // -----
  { { R, 0 }, { R-R/2, R/4 } },  // ----->
  { { R, 0 }, { R-R/2, -R/4 } },
  { { -R+R/8, 0 }, { -R-R/8, R/4 } }, // >---->
  { { -R+R/8, 0 }, { -R-R/8, -R/4 } },
  { { -R+3*R/8, 0 }, { -R+R/8, R/4 } }, // >>--->
  { { -R+3*R/8, 0 }, { -R+R/8, -R/4 } }
  };

#undef R
#define NUMPLYRLINES (sizeof(player_arrow)/sizeof(mline_t))

#define R ((8*PLAYERRADIUS)/7)
static mline_t cheat_player_arrow[] = {
  { { -R+R/8, 0 }, { R, 0 } }, // -----
  { { R, 0 }, { R-R/2, R/6 } },  // ----->
  { { R, 0 }, { R-R/2, -R/6 } },
  { { -R+R/8, 0 }, { -R-R/8, R/6 } }, // >----->
  { { -R+R/8, 0 }, { -R-R/8, -R/6 } },
  { { -R+3*R/8, 0 }, { -R+R/8, R/6 } }, // >>----->
  { { -R+3*R/8, 0 }, { -R+R/8, -R/6 } },
  { { -R/2, 0 }, { -R/2, -R/6 } }, // >>-d--->
  { { -R/2, -R/6 }, { -R/2+R/6, -R/6 } },
  { { -R/2+R/6, -R/6 }, { -R/2+R/6, R/4 } },
  { { -R/6, 0 }, { -R/6, -R/6 } }, // >>-dd-->
  { { -R/6, -R/6 }, { 0, -R/6 } },
  { { 0, -R/6 }, { 0, R/4 } },
  { { R/6, R/4 }, { R/6, -R/7 } }, // >>-ddt->
  { { R/6, -R/7 }, { R/6+R/32, -R/7-R/32 } },
  { { R/6+R/32, -R/7-R/32 }, { R/6+R/10, -R/7 } }
  };
#undef R
#define NUMCHEATPLYRLINES (sizeof(cheat_player_arrow)/sizeof(mline_t))

#define R (FRACUNIT)
static mline_t thintriangle_guy[] = {
  { { -.5*R, -.7*R }, { R, 0 } },
  { { R, 0 }, { -.5*R, .7*R } },
  { { -.5*R, .7*R }, { -.5*R, -.7*R } }
  };
#undef R
#define NUMTHINTRIANGLEGUYLINES (sizeof(thintriangle_guy)/sizeof(mline_t))

#endif
