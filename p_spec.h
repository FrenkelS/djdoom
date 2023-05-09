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

// P_spec.h

/*
===============================================================================

							P_SPEC

===============================================================================
*/

//
//	Animating textures and planes
//
typedef struct
{
	boolean		istexture;
	int32_t		picnum;
	int32_t		basepic;
	int32_t		numpics;
	int32_t		speed;
} anim_t;

//
//	source animation definition
//
typedef struct
{
	boolean	istexture;		// if false, it's a flat
	char	endname[9];
	char	startname[9];
	int32_t	speed;
} animdef_t;

#define	MAXANIMS		32

//
//	Animating line specials
//
#define	MAXLINEANIMS		64


//	Define values for map objects
#define	MO_TELEPORTMAN		14

// at game start
void	P_InitPicAnims (void);

// at map load
void	P_SpawnSpecials (void);

// every tic
void 	P_UpdateSpecials (void);

// when needed
boolean	P_UseSpecialLine ( mobj_t *thing, line_t *line, int32_t side);
void	P_ShootSpecialLine ( mobj_t *thing, line_t *line);
void 	P_CrossSpecialLine (int32_t linenum, int32_t side, mobj_t *thing);

void 	P_PlayerInSpecialSector (player_t *player);

boolean	twoSided(int32_t sector,int32_t line);
sector_t *getSector(int32_t currentSector,int32_t line,int32_t side);
side_t	*getSide(int32_t currentSector,int32_t line, int32_t side);
fixed_t	P_FindLowestFloorSurrounding(sector_t *sec);
fixed_t	P_FindHighestFloorSurrounding(sector_t *sec);
fixed_t	P_FindNextHighestFloor(sector_t *sec,int32_t currentheight);
fixed_t	P_FindLowestCeilingSurrounding(sector_t *sec);
fixed_t	P_FindHighestCeilingSurrounding(sector_t *sec);
int32_t	P_FindSectorFromLineTag(line_t	*line,int32_t start);
int32_t	P_FindMinSurroundingLight(sector_t *sector,int32_t max);
sector_t *getNextSector(line_t *line,sector_t *sec);

//
//	SPECIAL
//
boolean EV_DoDonut(line_t *line);

/*
===============================================================================

							P_LIGHTS

===============================================================================
*/
typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	int32_t		count;
	int32_t		maxlight;
	int32_t		minlight;
} fireflicker_t;

typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	int32_t		count;
	int32_t		maxlight;
	int32_t		minlight;
	int32_t		maxtime;
	int32_t		mintime;
} lightflash_t;

typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	int32_t		count;
	int32_t		minlight;
	int32_t		maxlight;
	int32_t		darktime;
	int32_t		brighttime;
} strobe_t;

typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	int32_t		minlight;
	int32_t		maxlight;
	int32_t		direction;
} glow_t;

#define GLOWSPEED		8
#define	STROBEBRIGHT	5
#define	FASTDARK		15
#define	SLOWDARK		35

void    P_SpawnFireFlicker (sector_t *sector);
void	T_LightFlash (lightflash_t *flash);
void	P_SpawnLightFlash (sector_t *sector);
void	T_StrobeFlash (strobe_t *flash);
void 	P_SpawnStrobeFlash (sector_t *sector, int32_t fastOrSlow, int32_t inSync);
void	EV_StartLightStrobing(line_t *line);
void	EV_TurnTagLightsOff(line_t	*line);
void	EV_LightTurnOn(line_t *line, int32_t bright);
void	T_Glow(glow_t *g);
void	P_SpawnGlowingLight(sector_t *sector);

/*
===============================================================================

							P_SWITCH

===============================================================================
*/
typedef struct
{
	char	name1[9];
	char	name2[9];
	int16_t	episode;
} switchlist_t;

typedef enum
{
	top,
	middle,
	bottom
} bwhere_e;

typedef struct
{
	line_t		*line;
	bwhere_e	where;
	int32_t		btexture;
	int32_t		btimer;
	mobj_t		*soundorg;
} button_t;

#define	MAXSWITCHES	50		// max # of wall switches in a level
#define	MAXBUTTONS	16		// 4 players, 4 buttons each at once, max.
#define BUTTONTIME	TICRATE	// 1 second

extern	button_t	buttonlist[MAXBUTTONS];	

void	P_ChangeSwitchTexture(line_t *line,int32_t useAgain);
void 	P_InitSwitchList(void);

/*
===============================================================================

							P_PLATS

===============================================================================
*/
typedef enum
{
	up,
	down,
	waiting,
	in_stasis
} plat_e;

typedef enum
{
	perpetualRaise,
	downWaitUpStay,
	raiseAndChange,
	raiseToNearestAndChange,
	blazeDWUS
} plattype_e;

typedef struct
{
	thinker_t	thinker;
	sector_t	*sector;
	fixed_t		speed;
	fixed_t		low;
	fixed_t		high;
	int32_t		wait;
	int32_t		count;
	plat_e		status;
	plat_e		oldstatus;
	boolean		crush;
	int32_t		tag;
	plattype_e	type;
} plat_t;

#define	PLATWAIT	3
#define	PLATSPEED	FRACUNIT
#define	MAXPLATS	30

extern	plat_t	*activeplats[MAXPLATS];

void	T_PlatRaise(plat_t	*plat);
boolean	EV_DoPlat(line_t *line,plattype_e type,int32_t amount);
void	P_AddActivePlat(plat_t *plat);
void	EV_StopPlat(line_t *line);

/*
===============================================================================

							P_DOORS

===============================================================================
*/
typedef enum
{
	normal,
	close30ThenOpen,
	close,
	open,
	raiseIn5Mins,
	blazeRaise,
	blazeOpen,
	blazeClose
} vldoor_e;

typedef struct
{
	thinker_t	thinker;
	vldoor_e	type;
	sector_t	*sector;
	fixed_t		topheight;
	fixed_t		speed;
	int32_t		direction;		// 1 = up, 0 = waiting at top, -1 = down
	int32_t		topwait;		// tics to wait at the top
								// (keep in case a door going down is reset)
	int32_t		topcountdown;	// when it reaches 0, start going down
} vldoor_t;
	
#define	VDOORSPEED	FRACUNIT*2
#define	VDOORWAIT		150

void	EV_VerticalDoor (line_t *line, mobj_t *thing);
boolean	EV_DoDoor (line_t *line, vldoor_e type);
boolean	EV_DoLockedDoor (line_t *line, vldoor_e type, mobj_t *thing);
void	T_VerticalDoor (vldoor_t *door);
void	P_SpawnDoorCloseIn30 (sector_t *sec);
void	P_SpawnDoorRaiseIn5Mins (sector_t *sec);

/*
===============================================================================

							P_CEILNG

===============================================================================
*/
typedef enum
{
	lowerToFloor,
	raiseToHighest,
	lowerAndCrush,
	crushAndRaise,
	fastCrushAndRaise,
	silentCrushAndRaise
} ceiling_e;

typedef struct
{
	thinker_t	thinker;
	ceiling_e	type;
	sector_t	*sector;
	fixed_t		bottomheight, topheight;
	fixed_t		speed;
	boolean		crush;
	int32_t		direction;		// 1 = up, 0 = waiting, -1 = down
	int32_t		tag;			// ID
	int32_t		olddirection;
} ceiling_t;

#define	CEILSPEED		FRACUNIT
#define	CEILWAIT		150
#define MAXCEILINGS		30

extern	ceiling_t	*activeceilings[MAXCEILINGS];

boolean	EV_DoCeiling (line_t *line, ceiling_e  type);
void	T_MoveCeiling (ceiling_t *ceiling);
void	P_AddActiveCeiling(ceiling_t *c);
void	EV_CeilingCrushStop(line_t	*line);

/*
===============================================================================

							P_FLOOR

===============================================================================
*/
typedef enum
{
	lowerFloor,			// lower floor to highest surrounding floor
	lowerFloorToLowest,	// lower floor to lowest surrounding floor
	turboLower,			// lower floor to highest surrounding floor VERY FAST
	raiseFloor,			// raise floor to lowest surrounding CEILING
	raiseFloorToNearest,	// raise floor to next highest surrounding floor
	raiseToTexture,		// raise floor to shortest height texture around it
	lowerAndChange,		// lower floor to lowest surrounding floor and change
						// floorpic
	raiseFloor24,
	raiseFloor24AndChange,
	raiseFloorCrush,
	raiseFloorTurbo,	// raise to next highest floor, turbo-speed
	donutRaise,
	raiseFloor512
} floor_e;

typedef struct
{
	thinker_t	thinker;
	floor_e		type;
	boolean		crush;
	sector_t	*sector;
	int32_t		direction;
	int32_t		newspecial;
	int16_t		texture;
	fixed_t		floordestheight;
	fixed_t		speed;
} floormove_t;

#define	FLOORSPEED	FRACUNIT

typedef enum
{
	ok,
	crushed,
	pastdest
} result_e;

result_e	T_MovePlane(sector_t *sector,fixed_t speed,
			fixed_t dest,boolean crush,int32_t floorOrCeiling,int32_t direction);

boolean	EV_BuildStairs(line_t *line, boolean build8);
boolean	EV_DoFloor(line_t *line,floor_e floortype);
void	T_MoveFloor(floormove_t *floor);

/*
===============================================================================

							P_TELEPT

===============================================================================
*/

void EV_Teleport(line_t *line, int32_t side, mobj_t *thing);
