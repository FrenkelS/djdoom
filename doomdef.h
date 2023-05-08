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

// DoomDef.h

#ifndef __DOOMDEF__
#define __DOOMDEF__
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"

// VERSIONS RESTORATION
// This *must* be included (near) the beginning for every compilation unit
#include "gamever.h"

#if (APPVER_DOOMREV < AV_DR_DM17)
#define VERSION 106
#elif (APPVER_DOOMREV < AV_DR_DM18FR)
#define VERSION 107
#elif (APPVER_DOOMREV < AV_DR_DM19)
#define VERSION 108
#else
#define VERSION 109
#endif

// if rangecheck is undefined, most parameter validation debugging code
// will not be compiled
#define RANGECHECK

// all external data is defined here
#include "doomdata.h"

// all important printed strings
#include "dstrings.h"

// header generated by multigen utility
#include "info.h"

#define UNUSED(x)	(x = x)	// for pesky compiler / lint warnings

extern byte *destview, *destscreen;	// PC direct to screen pointers

//
// most key data are simple ascii (uppercased)
//
#define	KEY_RIGHTARROW		0xae
#define	KEY_LEFTARROW		0xac
#define	KEY_UPARROW			0xad
#define	KEY_DOWNARROW		0xaf
#define	KEY_ESCAPE			27
#define	KEY_ENTER			13
#define KEY_TAB				9
#define	KEY_F1				(0x80+0x3b)
#define	KEY_F2				(0x80+0x3c)
#define	KEY_F3				(0x80+0x3d)
#define	KEY_F4				(0x80+0x3e)
#define	KEY_F5				(0x80+0x3f)
#define	KEY_F6				(0x80+0x40)
#define	KEY_F7				(0x80+0x41)
#define	KEY_F8				(0x80+0x42)
#define	KEY_F9				(0x80+0x43)
#define	KEY_F10				(0x80+0x44)
#define	KEY_F11				(0x80+0x57)
#define	KEY_F12				(0x80+0x58)

#define	KEY_BACKSPACE		127
#define	KEY_PAUSE			0xff

#define KEY_EQUALS			0x3d
#define KEY_MINUS			0x2d

#define	KEY_RSHIFT			(0x80+0x36)
#define	KEY_RCTRL			(0x80+0x1d)
#define	KEY_RALT			(0x80+0x38)

#define	KEY_LALT			KEY_RALT



#define MAXCHAR ((char)0x7f)
#define MAXSHORT ((short)0x7fff)
#define MAXINT	((int)0x7fffffff)	/* max pos 32-bit int */
#define MAXLONG ((int32_t)0x7fffffff)

#define MINCHAR ((char)0x80)
#define MINSHORT ((short)0x8000)
#define MININT 	((int)0x80000000)	/* max negative 32-bit integer */
#define MINLONG ((int32_t)0x80000000)

#define	FINEANGLES			8192
#define	FINEMASK			(FINEANGLES-1)
#define	ANGLETOFINESHIFT	19	// 0x100000000 to 0x2000

#if APPVER_CHEX
#define	SAVEGAMENAME "chexsav"
#else
#define	SAVEGAMENAME "doomsav"
#endif

/*
===============================================================================

						GLOBAL TYPES

===============================================================================
*/

#define MAXPLAYERS	4
#define TICRATE		35			// number of tics / second

#define	FRACBITS		16
#define	FRACUNIT		(1<<FRACBITS)
typedef int fixed_t;

#define ANGLE_1		0x01000000
#define ANGLE_45	0x20000000
#define ANGLE_90	0x40000000
#define ANGLE_180	0x80000000
#define ANGLE_MAX	0xffffffff

#define	ANG45	0x20000000
#define	ANG90	0x40000000
#define	ANG180	0x80000000
#define	ANG270	0xc0000000

typedef unsigned angle_t;

typedef enum
{
	sk_baby,
	sk_easy,
	sk_medium,
	sk_hard,
	sk_nightmare
} skill_t;

typedef enum
{
	ev_keydown,
	ev_keyup,
	ev_mouse,
	ev_joystick
} evtype_t;

typedef struct
{
	evtype_t	type;
	int			data1;		// keys / mouse/joystick buttons
	int			data2;		// mouse/joystick x move
	int			data3;		// mouse/joystick y move
} event_t;

typedef struct
{
	char		forwardmove;		// *2048 for move
	char		sidemove;			// *2048 for move
	short		angleturn;			// <<16 for angle delta
	short		consistancy;		// checks for net game
	byte		chatchar;
	byte		buttons;
} ticcmd_t;

#define	BT_ATTACK		1
#define	BT_USE			2
#define	BT_CHANGE		4			// if true, the next 3 bits hold weapon num
#define	BT_WEAPONMASK	(8+16+32)
#define	BT_WEAPONSHIFT	3

#define BT_SPECIAL		128			// game events, not really buttons
#define	BTS_SAVEMASK	(4+8+16)
#define	BTS_SAVESHIFT	2
#define	BT_SPECIALMASK	3
#define	BTS_PAUSE		1			// pause the game
#define	BTS_SAVEGAME	2			// save the game at each console
// savegame slot numbers occupy the second byte of buttons

typedef enum
{
	GS_LEVEL,
	GS_INTERMISSION,
	GS_FINALE,
	GS_DEMOSCREEN
} gamestate_t;

typedef enum
{
	ga_nothing,
	ga_loadlevel,
	ga_newgame,
	ga_loadgame,
	ga_savegame,
	ga_playdemo,
	ga_completed,
	ga_victory,
	ga_worlddone,
	ga_screenshot
} gameaction_t;

/*
===============================================================================

							MAPOBJ DATA

===============================================================================
*/

// think_t is a function pointer to a routine to handle an actor
typedef void (*think_t) ();

typedef struct thinker_s
{
	struct		thinker_s	*prev, *next;
	think_t		function;
} thinker_t;

struct player_s;

typedef struct mobj_s
{
	thinker_t		thinker;			// thinker links

// info for drawing
	fixed_t			x,y,z;
	struct	mobj_s	*snext, *sprev;		// links in sector (if needed)
	angle_t			angle;
	spritenum_t		sprite;				// used to find patch_t and flip value
	int				frame;				// might be ord with FF_FULLBRIGHT

// interaction info
	struct mobj_s	*bnext, *bprev;		// links in blocks (if needed)
	struct subsector_s	*subsector;
	fixed_t			floorz, ceilingz;	// closest together of contacted secs
	fixed_t			radius, height;		// for movement checking
	fixed_t			momx, momy, momz;	// momentums

	int				validcount;			// if == validcount, already checked

	mobjtype_t		type;
	mobjinfo_t		*info;				// &mobjinfo[mobj->type]
	int				tics;				// state tic counter
	state_t			*state;
	int				flags;
	int				health;
	int				movedir;		// 0-7
	int				movecount;		// when 0, select a new dir
	struct mobj_s	*target;		// thing being chased/attacked (or NULL)
									// also the originator for missiles
	int				reactiontime;	// if non 0, don't attack yet
									// used by player to freeze a bit after
									// teleporting
	int				threshold;		// if >0, the target will be chased
									// no matter what (even if shot)
	struct player_s	*player;		// only valid if type == MT_PLAYER
	int				lastlook;		// player number last looked for

	mapthing_t		spawnpoint;		// for nightmare respawn
	struct mobj_s	*tracer;		// thing being chased/attacked for tracers
} mobj_t;

// each sector has a degenmobj_t in it's center for sound origin purposes
typedef struct
{
	thinker_t		thinker;		// not used for anything
	fixed_t			x,y,z;
} degenmobj_t;

//
// frame flags
//
#define	FF_FULLBRIGHT	0x8000		// flag in thing->frame
#define FF_FRAMEMASK	0x7fff

// --- mobj.flags ---

#define	MF_SPECIAL		1			// call P_SpecialThing when touched
#define	MF_SOLID		2
#define	MF_SHOOTABLE	4
#define	MF_NOSECTOR		8			// don't use the sector links
									// (invisible but touchable)
#define	MF_NOBLOCKMAP	16			// don't use the blocklinks
									// (inert but displayable)
#define	MF_AMBUSH		32
#define	MF_JUSTHIT		64			// try to attack right back
#define	MF_JUSTATTACKED	128			// take at least one step before attacking
#define	MF_SPAWNCEILING	256			// hang from ceiling instead of floor
#define	MF_NOGRAVITY	512			// don't apply gravity every tic

// movement flags
#define	MF_DROPOFF		0x400		// allow jumps from high places
#define	MF_PICKUP		0x800		// for players to pick up items
#define	MF_NOCLIP		0x1000		// player cheat
#define	MF_SLIDE		0x2000		// keep info about sliding along walls
#define	MF_FLOAT		0x4000		// allow moves to any height, no gravity
#define	MF_TELEPORT		0x8000		// don't cross lines or look at heights
#define MF_MISSILE		0x10000		// don't hit same species, explode on block

#define	MF_DROPPED		0x20000		// dropped by a demon, not level spawned
#define	MF_SHADOW		0x40000		// use fuzzy draw (shadow demons / invis)
#define	MF_NOBLOOD		0x80000		// don't bleed when shot (use puff)
#define	MF_CORPSE		0x100000	// don't stop moving halfway off a step
#define	MF_INFLOAT		0x200000	// floating to a height for a move, don't
									// auto float to target's height

#define	MF_COUNTKILL	0x400000	// count towards intermission kill total
#define	MF_COUNTITEM	0x800000	// count towards intermission item total

#define	MF_SKULLFLY		0x1000000	// skull in flight
#define	MF_NOTDMATCH	0x2000000	// don't spawn in death match (key cards)

#define	MF_TRANSLATION	0xc000000	// if 0x4 0x8 or 0xc, use a translation
#define	MF_TRANSSHIFT	26			// table for player colormaps

//=============================================================================
typedef enum
{
	PST_LIVE,			// playing
	PST_DEAD,			// dead on the ground
	PST_REBORN			// ready to restart
} playerstate_t;

// psprites are scaled shapes directly on the view screen
// coordinates are given for a 320*200 view screen
typedef enum
{
	ps_weapon,
	ps_flash,
	NUMPSPRITES
} psprnum_t;

typedef struct
{
	state_t	*state;		// a NULL state means not active
	int		tics;
	fixed_t	sx, sy;
} pspdef_t;

typedef enum
{
	it_bluecard,
	it_yellowcard,
	it_redcard,
	it_blueskull,
	it_yellowskull,
	it_redskull,
	NUMCARDS
} card_t;

typedef enum
{
	wp_fist,
	wp_pistol,
	wp_shotgun,
	wp_chaingun,
	wp_missile,
	wp_plasma,
	wp_bfg,
	wp_chainsaw,
	wp_supershotgun,
	NUMWEAPONS,
	wp_nochange
} weapontype_t;

typedef enum
{
	am_clip,		// pistol / chaingun
	am_shell,		// shotgun
	am_cell,		// BFG
	am_misl,		// missile launcher
	NUMAMMO,
	am_noammo		// chainsaw / fist
} ammotype_t;

typedef struct
{
	ammotype_t ammo;
	int upstate;
	int downstate;
	int readystate;
	int atkstate;
	int flashstate;
} weaponinfo_t;

extern weaponinfo_t weaponinfo[NUMWEAPONS];

typedef enum
{
	pw_invulnerability,
	pw_strength,
	pw_invisibility,
	pw_ironfeet,
	pw_allmap,
	pw_infrared,
	NUMPOWERS
} powertype_t;

#define	INVULNTICS (30*TICRATE)
#define	INVISTICS (60*TICRATE)
#define	INFRATICS (120*TICRATE)
#define	IRONTICS (60*TICRATE)

/*
================
=
= player_t
=
================
*/


typedef struct player_s
{
	mobj_t *mo;
	playerstate_t playerstate;
	ticcmd_t cmd;

	fixed_t		viewz;					// focal origin above r.z
	fixed_t		viewheight;				// base height above floor for viewz
	fixed_t		deltaviewheight;		// squat speed
	fixed_t		bob;					// bounded/scaled total momentum
	
	int			health;					// only used between levels, mo->health
										// is used during levels
	int			armorpoints, armortype;	// armor type is 0-2

	int			powers[NUMPOWERS];
	boolean		cards[NUMCARDS];
	boolean		backpack;
	signed int			frags[MAXPLAYERS];		// kills of other players
	weapontype_t	readyweapon;
	weapontype_t	pendingweapon;		// wp_nochange if not changing
	boolean		weaponowned[NUMWEAPONS];
	int			ammo[NUMAMMO];
	int			maxammo[NUMAMMO];
	int			attackdown, usedown;	// true if button down last tic
	int			cheats;					// bit flags

	int			refire;					// refired shots are less accurate

	int			killcount, itemcount, secretcount;		// for intermission
	char		*message;				// hint messages
	int			damagecount, bonuscount;// for screen flashing
	mobj_t		*attacker;				// who did damage (NULL for floors)
	int			extralight;				// so gun flashes light up areas
	int			fixedcolormap;			// can be set to REDCOLORMAP, etc
	int			colormap;				// 0-3 for which color to draw player
	pspdef_t	psprites[NUMPSPRITES];	// view sprites (gun, etc)
	boolean		didsecret;				// true if secret level has been done
} player_t;

#define CF_NOCLIP		1
#define	CF_GODMODE		2
#define	CF_NOMOMENTUM	4 // not really a cheat, just a debug aid


#define		BACKUPTICS		12		// CHANGED FROM 12 !?!?

typedef struct
{
	unsigned	checksum;					// high bit is retransmit request
	byte		retransmitfrom;				// only valid if NCMD_RETRANSMIT
	byte		starttic;
	byte		player, numtics;
	ticcmd_t	cmds[BACKUPTICS];
} doomdata_t;

typedef struct
{
	int32_t	id;
	short	intnum;			// DOOM executes an int to execute commands

// communication between DOOM and the driver
	short	command;		// CMD_SEND or CMD_GET
	short	remotenode;		// dest for send, set by get (-1 = no packet)
	short	datalength;		// bytes in doomdata to be sent

// info common to all nodes
	short	numnodes;		// console is allways node 0
	short	ticdup;			// 1 = no duplication, 2-5 = dup for slow nets
	short	extratics;		// 1 = send a backup tic in every packet
	short	deathmatch;		// 1 = deathmatch
	short	savegame;		// -1 = new game, 0-5 = load savegame
	short	episode;		// 1-3
	short	map;			// 1-9
	short	skill;			// 1-5

// info specific to this node
	short	consoleplayer;
	short	numplayers;
	short	angleoffset;	// 1 = left, 0 = center, -1 = right
	short	drone;			// 1 = drone

// packet data to be sent
	doomdata_t	data;
} doomcom_t;

#define	DOOMCOM_ID		0x12345678l

extern	doomcom_t		*doomcom;

#define	MAXNETNODES		8			// max computers in a game

#define	CMD_SEND	1
#define	CMD_GET		2

#define	SBARHEIGHT	32			// status bar height at bottom of screen


/*
===============================================================================

					GLOBAL VARIABLES

===============================================================================
*/

#define MAXEVENTS 64

extern event_t events[MAXEVENTS];
extern int eventhead;
extern int eventtail;

extern fixed_t finesine[5*FINEANGLES/4];
extern fixed_t *finecosine;

extern gameaction_t gameaction;

extern boolean paused;

extern boolean usergame;

extern boolean nomonsters; // checkparm of -nomonsters
extern boolean respawnparm; // checkparm of -respawn
extern boolean fastparm; // checkparm of -fastparm

extern boolean shareware; // true if main WAD is the shareware version
extern boolean registered;
extern boolean commercial;
#if (APPVER_DOOMREV >= AV_DR_DM18FR)
extern boolean french;
#if (APPVER_DOOMREV >= AV_DR_DM19F)
extern boolean plutonia;
extern boolean tnt;
#endif
#endif

extern boolean devparm; // started game with -devparm

extern boolean deathmatch; // only if started as net death

extern boolean netgame; // only true if >1 player

extern boolean playeringame[MAXPLAYERS];

extern int consoleplayer; // player taking events and displaying

extern int displayplayer;

extern int viewangleoffset;	// ANG90 = left side, ANG270 = right

extern player_t players[MAXPLAYERS];

extern	boolean		singletics;			// debug flag to cancel adaptiveness

extern	int			maxammo[NUMAMMO];

extern	boolean		demoplayback;
extern	boolean		demorecording;
extern	int			skytexture;

extern	gamestate_t	gamestate;
extern	skill_t		gameskill;
extern	boolean		respawnmonsters;
extern	int			gameepisode;
extern	int			gamemap;
extern 	int 			prevmap;
extern	int			totalkills, totalitems, totalsecret;	// for intermission
extern	int			leveltime;			// tics in game play for par

extern	ticcmd_t	netcmds[MAXPLAYERS][BACKUPTICS];
extern int ticdup;

#define	MAXNETNODES		8
extern int rndindex;
extern int gametic, maketic;

#define SAVEGAMESIZE 0x2c000
#define SAVESTRINGSIZE 24
extern byte *save_p;

extern mapthing_t *deathmatch_p;
extern mapthing_t deathmatchstarts[10];
extern mapthing_t playerstarts[MAXPLAYERS];

extern int viewwindowx;
extern int viewwindowy;
extern int viewwidth;
extern int scaledviewwidth;
extern int viewheight;

extern int mouseSensitivity;

extern gamestate_t wipegamestate;

extern boolean precache; // if true, load all graphics at level load

extern byte *screens[5]; // off screen work buffer, from V_video.c

extern boolean automapactive;
extern boolean menuactive;
extern boolean nodrawers;
extern boolean viewactive;
extern boolean singledemo; // quit after playing a demo from cmdline
extern boolean modifiedgame;
extern char wadfile[1024];
extern char basedefault[1024];
extern FILE *debugfile;
extern int bodyqueslot;
extern skill_t startskill;
extern int startepisode;
extern int startmap;
extern boolean autostart;

/*
===============================================================================

					GLOBAL FUNCTIONS

===============================================================================
*/


fixed_t	FixedMul (fixed_t a, fixed_t b);
fixed_t	FixedDiv (fixed_t a, fixed_t b);

#ifdef __BIG_ENDIAN__
short ShortSwap(short);
int32_t LongSwap(int32_t);
#define SHORT(x)	ShortSwap(x)
#define LONG(x)		LongSwap(x)
#else
#define SHORT(x)	(x)
#define LONG(x)		(x)
#endif


//-----------
//MEMORY ZONE
//-----------
// tags < 100 are not overwritten until freed
#define	PU_STATIC		1			// static entire execution time
#define	PU_SOUND		2			// static while playing
#define	PU_MUSIC		3			// static while playing
#define	PU_DAVE			4			// anything else Dave wants static
#define	PU_LEVEL		50			// static until level exited
#define	PU_LEVSPEC		51			// a special thinker in a level
// tags >= 100 are purgable whenever needed
#define	PU_PURGELEVEL	100
#define	PU_CACHE		101

void	Z_Init (void);
void 	*Z_Malloc (int size, int tag, void *ptr);
void 	Z_Free (void *ptr);
void 	Z_FreeTags (int lowtag, int hightag);
void	Z_CheckHeap (void);
void	Z_ChangeTag2 (void *ptr, int tag);


typedef struct memblock_s
{
	int                     size;           // including the header and possibly tiny fragments
	void            **user;         // NULL if a free block
	int                     tag;            // purgelevel
	int                     id;                     // should be ZONEID
	struct memblock_s       *next, *prev;
} memblock_t;

#define Z_ChangeTag(p,t) \
{ \
if (( (memblock_t *)( (byte *)(p) - sizeof(memblock_t)))->id!=0x1d4a11) \
	I_Error("Z_CT at "__FILE__":%i",__LINE__); \
Z_ChangeTag2(p,t); \
};

//-------
//WADFILE
//-------
typedef struct
{
	char		name[8];
	int			handle,position,size;
} lumpinfo_t;

extern	void**		_lumpcache; // temp hack
extern lumpinfo_t *lumpinfo;

void	W_InitMultipleFiles (char **filenames);
void	W_Reload (void);

int		W_CheckNumForName (char *name);
int		W_GetNumForName (char *name);

int		W_LumpLength (int lump);
void	W_ReadLump (int lump, void *dest);

void	*W_CacheLumpNum (int lump, int tag);
void	*W_CacheLumpName (char *name, int tag);




//----------
//BASE LEVEL
//----------
void D_DoomMain (void);

// static void D_DoomLoop (void);
// not a globally visible function, just included for source reference
// called by D_DoomMain, never exits
// manages timing and IO
// calls all ?_Responder, ?_Ticker, and ?_Drawer functions
// calls I_GetTime, I_StartFrame, and I_StartTic

void D_PostEvent (event_t *ev);
// called by IO functions when input is detected

void NetUpdate (void);
// create any new ticcmds and broadcast to other players

void D_QuitNetGame (void);
// broadcasts special packets to other players to notify of game exit

void TryRunTics (void);

void mprintf (char *);

void D_PageTicker (void);
void D_AdvanceDemo (void);
void D_StartTitle (void);

//---------
//SYSTEM IO
//---------

#define SCREENWIDTH  320
#define SCREENHEIGHT 200
#define PLANEWIDTH	(SCREENWIDTH/4)

byte *I_ZoneBase (int *size);
// called by startup code to get the ammount of memory to malloc
// for the zone management

int I_GetTime (void);
// called by D_DoomLoop
// returns current time in tics

void I_StartFrame (void);
// called by D_DoomLoop
// called before processing any tics in a frame (just after displaying a frame)
// time consuming syncronous operations are performed here (joystick reading)
// can call D_PostEvent

void I_StartTic (void);
// called by D_DoomLoop
// called before processing each tic in a frame
// quick syncronous operations are performed here
// can call D_PostEvent

// asyncronous interrupt functions should maintain private ques that are
// read by the syncronous functions to be converted into events

void I_Init (void);
// called by D_DoomMain
// determines the hardware configuration and sets up the video mode

void I_InitGraphics (void);

void I_InitNetwork (void);
void I_NetCmd (void);

void I_Error (char *error, ...);
// called by anything that can generate a terminal error
// bad exit with diagnostic message

void I_Quit (void);
// called by M_Responder when quit is selected
// clean exit, displays sell blurb

void I_SetPalette (byte *palette);
// takes full 8 bit values

void I_UpdateNoBlit (void);

void I_FinishUpdate (void);

void I_WaitVBL(int count);
// wait for vertical retrace or pause a bit

void I_ReadScreen (byte *scr);

void I_BeginRead (void);
void I_EndRead (void);

ticcmd_t *I_BaseTiccmd(void);
// Either returns a null ticcmd,
// or calls a loadable driver to build it.
// This ticcmd will then be modified by the gameloop
// for normal input.

//----
//GAME
//----

void G_DeathMatchSpawnPlayer (int playernum);

void G_InitNew (skill_t skill, int episode, int map);

void G_DeferedInitNew (skill_t skill, int episode, int map);
// can be called by the startup code or M_Responder
// a normal game starts at map 1, but a warp test can start elsewhere

void G_DeferedPlayDemo (char *demo);

void G_LoadGame (char *name);
// can be called by the startup code or M_Responder
// calls P_SetupLevel or W_EnterWorld

void G_SaveGame (int slot, char *description);
// called by M_Responder

void G_RecordDemo (char *name);
// only called by startup code

void G_BeginRecording (void);

void G_PlayDemo (char *name);
void G_TimeDemo (char *name);
void G_CheckDemoStatus (void);

void G_ExitLevel (void);
void G_SecretExitLevel (void);

void G_WorldDone (void);

void G_Ticker (void);
void G_Responder (event_t *ev);

void G_ScreenShot (void);

//-----
//PLAY
//-----

void P_Ticker (void);
// called by C_Ticker
// can call G_PlayerExited
// carries out all thinking of monsters and players

void P_SetupLevel (int episode, int map);
// called by W_Ticker

void P_Init (void);
// called by startup code

void P_ArchivePlayers (void);
void P_UnArchivePlayers (void);
void P_ArchiveWorld (void);
void P_UnArchiveWorld (void);
void P_ArchiveThinkers (void);
void P_UnArchiveThinkers (void);
void P_ArchiveSpecials (void);
void P_UnArchiveSpecials (void);
// load / save game routines

//-------
//REFRESH
//-------

void R_RenderPlayerView (player_t *player);
// called by G_Drawer

void R_Init (void);
// called by startup code

// Rendering function.
void R_FillBackScreen (void);

void R_DrawViewBorder (void);
// if the view size is not full screen, draws a border around it

void R_SetViewSize (int blocks, int detail);
// called by M_Responder

int	R_FlatNumForName (char *name);

int	R_TextureNumForName (char *name);
int	R_CheckTextureNumForName (char *name);
// called by P_Ticker for switches and animations
// returns the texture number for the texture name

//----
//MISC
//----
extern	int		myargc;
extern	char	**myargv;

int	M_CheckParm (char *check);
// returns the position of the given parameter in the arg list (0 if not found)

int M_Random (void);
// returns a number from 0 to 255
int P_Random (void);
// as M_Random, but used only by the play simulation

void M_ClearRandom (void);
// fix randoms for demos

void M_ClearBox (fixed_t *box);
void M_AddToBox (fixed_t *box, fixed_t x, fixed_t y);
// bounding box functions

void M_WriteFile (char const *name, void *source, int length);
void M_ReadFile (char const *name, byte **buffer);

void M_ScreenShot (void);

void M_LoadDefaults (void);

void M_SaveDefaults (void);

//------
// VIDEO
//------

extern int dirtybox[4];
extern byte gammatable[5][256];
extern int usegamma;

void V_Init(void); // Allocates buffer screens, call before R_Init
void V_CopyRect(int x, int scry, int width, int height, int desty);
void V_DrawPatch(int x, int y, int scrn, patch_t *patch);
void V_DrawPatchDirect(int x, int y, patch_t *patch);
void V_DrawBlock(byte *src); // Draw a linear block of pixels into the view buffer
void V_MarkRect(int x, int y, int width, int height);

/////////////////////////////////////////////////////

//
// FINALE
//

// Called by main loop.
boolean F_Responder (event_t* ev);

// Called by main loop.
void F_Ticker (void);

// Called by main loop.
void F_Drawer (void);


void F_StartFinale (void);

//
// MENUS
//
// Called by main loop,
// saves config file and calls I_Quit when user exits.
// Even when the menu is not displayed,
// this can resize the view and change game parameters.
// Does all the real work of the menu interaction.
boolean M_Responder (event_t *ev);


// Called by main loop,
// only used for menu (skull cursor) animation.
void M_Ticker (void);

// Called by main loop,
// draws the menus directly into the screen buffer.
void M_Drawer (void);

// Called by D_DoomMain,
// loads the config file.
void M_Init (void);

// Called by intro code to force menu up upon a keypress,
// does nothing if menu is already up.
void M_StartControlPanel (void);

//
// INTERMISSION
// Structure passed e.g. to WI_Start(wb)
//
typedef struct
{
    boolean	in;	// whether the player is in game
    
    // Player stats, kills, collected items etc.
    int		skills;
    int		sitems;
    int		ssecret;
    int		stime; 
    int		frags[4];
    int		score;	// current score on entry, modified on return
  
} wbplayerstruct_t;

typedef struct
{
    int		epsd;	// episode # (0-2)

    // if true, splash the secret level
    boolean	didsecret;
    
    // previous and next levels, origin 0
    int		last;
    int		next;	
    
    int		maxkills;
    int		maxitems;
    int		maxsecret;
    int		maxfrags;

    // the par time
    int		partime;
    
    // index of this player in game
    int		pnum;	

    wbplayerstruct_t	plyr[MAXPLAYERS];

} wbstartstruct_t;

// Intermission stats.
// Parameters for world map / intermission.
extern  wbstartstruct_t		wminfo;

// States for the intermission

typedef enum
{
    NoState = -1,
    StatCount,
    ShowNextLoc

} stateenum_t;

// Called by main loop, animate the intermission.
void WI_Ticker (void);

// Called by main loop,
// draws the intermission directly into the screen buffer.
void WI_Drawer (void);

// Setup for an intermission screen.
void WI_Start (wbstartstruct_t * wbstartstruct);

// Called by main loop.
boolean AM_Responder (event_t* ev);

// Called by main loop.
void AM_Ticker (void);

// Called by main loop,
// called instead of view drawer if automap active.
void AM_Drawer (void);

// Called to force the automap to quit
// if the level is completed while it is up.
void AM_Stop (void);

// Size of statusbar.
#define ST_HEIGHT	32
#define ST_WIDTH	SCREENWIDTH
#define ST_Y		(SCREENHEIGHT - ST_HEIGHT)

//
// STATUS BAR
//

// Called by main loop.
void ST_Responder (event_t* ev);

// Called by main loop.
void ST_Ticker (void);

// Called by main loop.
void ST_Drawer (boolean fullscreen, boolean refresh);

// Called when the console player is spawned on each level.
void ST_Start (void);

// Called by startup code.
void ST_Init (void);



// States for status bar code.
typedef enum
{
    AutomapState,
    FirstPersonState
    
} st_stateenum_t;


// States for the chat code.
typedef enum
{
    StartChatState,
    WaitDestState,
    GetChatState
    
} st_chatstateenum_t;


//
// HEADS UP TEXT
//

void HU_Init(void);
void HU_Start(void);

boolean HU_Responder(event_t *ev);

void HU_Ticker(void);
void HU_Drawer(void);
char HU_dequeueChatChar(void);
void HU_Erase(void);


#include "sounds.h"

#endif // __DOOMDEF__
