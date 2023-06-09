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

// G_game.c

#include "doomdef.h"
#include "p_local.h"
#include "soundst.h"

void G_CheckDemoStatus (void);
static void G_ReadDemoTiccmd (ticcmd_t *cmd);
static void G_WriteDemoTiccmd (ticcmd_t *cmd);
void G_PlayerReborn (int32_t player);
void G_InitNew (skill_t skill, int32_t episode, int32_t map);

static void G_DoReborn (int32_t playernum);

static void G_DoLoadLevel (void);
static void G_DoNewGame (void);
static void G_DoLoadGame (void);
static void G_DoPlayDemo (void);
static void G_DoCompleted (void);
static void G_DoWorldDone (void);
static void G_DoSaveGame (void);

void D_PageTicker(void);
void D_AdvanceDemo(void);


gameaction_t    gameaction;
gamestate_t     gamestate;
skill_t         gameskill;
boolean         respawnmonsters;
int32_t         gameepisode;
int32_t         gamemap;

boolean         paused;
static boolean         sendpause;              // send a pause event next tic
static boolean         sendsave;               // send a save event next tic
boolean         usergame;               // ok to save / end game

static boolean         timingdemo;             // if true, exit with report on completion
boolean         nodrawers;              // for comparative timing purposes 
static boolean         noblit;                 // for comparative timing purposes 
static int32_t             starttime;              // for comparative timing purposes

boolean         viewactive;

boolean         deathmatch;             // only if started as net death
boolean         netgame;                // only true if packets are broadcast
boolean         playeringame[MAXPLAYERS];
player_t        players[MAXPLAYERS];

int32_t             consoleplayer;          // player taking events and displaying
int32_t             displayplayer;          // view being displayed
int32_t             gametic;
static int32_t             levelstarttic;          // gametic at level start
int32_t             totalkills, totalitems, totalsecret;    // for intermission

static char            demoname[32];
boolean         demorecording;
boolean         demoplayback;
static boolean			netdemo;
static byte            *demobuffer, *demo_p, *demoend;
boolean         singledemo;             // quit after playing a demo from cmdline

boolean         precache = true;        // if true, load all graphics at start
 
wbstartstruct_t wminfo;               	// parms for world map / intermission 

static int16_t            consistancy[MAXPLAYERS][BACKUPTICS];

static byte            *savebuffer;
byte                   *save_p;


//
// controls (have defaults)
//
int32_t             key_right, key_left, key_up, key_down;
int32_t             key_strafeleft, key_straferight;
int32_t             key_fire, key_use, key_strafe, key_speed;

int32_t             mousebfire;
int32_t             mousebstrafe;
int32_t             mousebforward;

int32_t             joybfire;
int32_t             joybstrafe;
int32_t             joybuse;
int32_t             joybspeed;



#define MAXPLMOVE       (forwardmove[1])
 
#define TURBOTHRESHOLD	0x32

fixed_t         forwardmove[2] = {0x19, 0x32};
fixed_t         sidemove[2] = {0x18, 0x28};
static fixed_t         angleturn[3] = {640, 1280, 320};     // + slow turn
#define SLOWTURNTICS    6

#define NUMKEYS 256
static boolean         gamekeydown[NUMKEYS];
static int32_t         turnheld;                   // for accelerative turning


static boolean         mousearray[4];
static boolean         *mousebuttons = &mousearray[1];
	// allow [-1]
static int32_t         mousex, mousey;             // mouse values are used once
static int32_t         dclicktime, dclickstate, dclicks;
static int32_t         dclicktime2, dclickstate2, dclicks2;

static int32_t         joyxmove, joyymove;         // joystick values are repeated
static boolean         joyarray[5];
static boolean         *joybuttons = &joyarray[1];     // allow [-1]

static int32_t savegameslot;
static char    savedescription[32];
 
 
#define	BODYQUESIZE	32

static mobj_t *bodyque[BODYQUESIZE]; 
int32_t bodyqueslot; 
 
void *statcopy;				// for statistics driver


/*
====================
=
= G_BuildTiccmd
=
= Builds a ticcmd from all of the available inputs or reads it from the
= demo buffer.
= If recording a demo, write it out
====================
*/

void G_BuildTiccmd (ticcmd_t *cmd)
{
	int32_t         i;
	boolean         strafe, bstrafe;
	int32_t         speed, tspeed;
	int32_t         forward, side;

	ticcmd_t		*base;

	base = I_BaseTiccmd ();		// empty, or external driver
	memcpy (cmd,base,sizeof(*cmd)); 
	//cmd->consistancy =
	//      consistancy[consoleplayer][(maketic*ticdup)%BACKUPTICS];
	cmd->consistancy =
		consistancy[consoleplayer][maketic%BACKUPTICS];

//printf ("cons: %i\n",cmd->consistancy);

	strafe = gamekeydown[key_strafe] || mousebuttons[mousebstrafe]
		|| joybuttons[joybstrafe];
	speed = gamekeydown[key_speed] || joybuttons[joybspeed];

	forward = side = 0;

//
// use two stage accelerative turning on the keyboard and joystick
//
	if (joyxmove < 0 || joyxmove > 0
	|| gamekeydown[key_right] || gamekeydown[key_left])
		turnheld += ticdup;
	else
		turnheld = 0;
	if (turnheld < SLOWTURNTICS)
		tspeed = 2;             // slow turn
	else
		tspeed = speed;

//
// let movement keys cancel each other out
//
	if(strafe)
	{
		if (gamekeydown[key_right])
			side += sidemove[speed];
		if (gamekeydown[key_left])
			side -= sidemove[speed];
		if (joyxmove > 0)
			side += sidemove[speed];
		if (joyxmove < 0)
			side -= sidemove[speed];
	}
	else
	{
		if (gamekeydown[key_right])
			cmd->angleturn -= angleturn[tspeed];
		if (gamekeydown[key_left])
			cmd->angleturn += angleturn[tspeed];
		if (joyxmove > 0)
			cmd->angleturn -= angleturn[tspeed];
		if (joyxmove < 0)
			cmd->angleturn += angleturn[tspeed];
	}

	if (gamekeydown[key_up])
		forward += forwardmove[speed];
	if (gamekeydown[key_down])
		forward -= forwardmove[speed];
	if (joyymove < 0)
		forward += forwardmove[speed];
	if (joyymove > 0)
		forward -= forwardmove[speed];
	if (gamekeydown[key_straferight])
		side += sidemove[speed];
	if (gamekeydown[key_strafeleft])
		side -= sidemove[speed];

//
// buttons
//
	cmd->chatchar = HU_dequeueChatChar();

	if (gamekeydown[key_fire] || mousebuttons[mousebfire]
		|| joybuttons[joybfire])
		cmd->buttons |= BT_ATTACK;

	if (gamekeydown[key_use] || joybuttons[joybuse] )
	{
		cmd->buttons |= BT_USE;
		dclicks = 0;                    // clear double clicks if hit use button
	}

//
// chainsaw overrides 
//
	for(i = 0; i < NUMWEAPONS-1; i++)
	{
		if(gamekeydown['1'+i])
		{
			cmd->buttons |= BT_CHANGE;
			cmd->buttons |= i<<BT_WEAPONSHIFT;
			break;
		}
	}

//
// mouse
//
	if (mousebuttons[mousebforward])
	{
		forward += forwardmove[speed];
	}

//
// forward double click
//
	if (mousebuttons[mousebforward] != dclickstate && dclicktime > 1 )
	{
		dclickstate = mousebuttons[mousebforward];
		if (dclickstate)
			dclicks++;
		if (dclicks == 2)
		{
			cmd->buttons |= BT_USE;
			dclicks = 0;
		}
		else
			dclicktime = 0;
	}
	else
	{
		dclicktime += ticdup;
		if (dclicktime > 20)
		{
			dclicks = 0;
			dclickstate = 0;
		}
	}

//
// strafe double click
//
	bstrafe = mousebuttons[mousebstrafe]
|| joybuttons[joybstrafe];
	if (bstrafe != dclickstate2 && dclicktime2 > 1 )
	{
		dclickstate2 = bstrafe;
		if (dclickstate2)
			dclicks2++;
		if (dclicks2 == 2)
		{
			cmd->buttons |= BT_USE;
			dclicks2 = 0;
		}
		else
			dclicktime2 = 0;
	}
	else
	{
		dclicktime2 += ticdup;
		if (dclicktime2 > 20)
		{
			dclicks2 = 0;
			dclickstate2 = 0;
		}
	}

	forward += mousey;
	if (strafe)
		side += mousex*2;
	else
		cmd->angleturn -= mousex*0x8;

	mousex = mousey = 0;

	if (forward > MAXPLMOVE)
		forward = MAXPLMOVE;
	else if (forward < -MAXPLMOVE)
		forward = -MAXPLMOVE;
	if (side > MAXPLMOVE)
		side = MAXPLMOVE;
	else if (side < -MAXPLMOVE)
		side = -MAXPLMOVE;

	cmd->forwardmove += forward;
	cmd->sidemove += side;

//
// special buttons
//
	if (sendpause)
	{
		sendpause = false;
		cmd->buttons = BT_SPECIAL | BTS_PAUSE;
	}

	if (sendsave)
	{
		sendsave = false;
		cmd->buttons = BT_SPECIAL | BTS_SAVEGAME | (savegameslot<<BTS_SAVESHIFT);
	}

}


/*
==============
=
= G_DoLoadLevel
=
==============
*/

static void G_DoLoadLevel (void)
{
	int32_t             i;

#if (APPVER_DOOMREV >= AV_DR_DM19F2) // "Sky never changes in Doom II" bug fix
	if (commercial)
	{
		skytexture = R_TextureNumForName ("SKY3");
		if (gamemap < 12)
			skytexture = R_TextureNumForName ("SKY1");
		else
			if (gamemap < 21)
				skytexture = R_TextureNumForName ("SKY2");
	}
#endif

	levelstarttic = gametic;        // for time calculation
	
	if (wipegamestate == GS_LEVEL) 
		wipegamestate = -1;             // force a wipe 

	gamestate = GS_LEVEL;
	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		if (playeringame[i] && players[i].playerstate == PST_DEAD)
			players[i].playerstate = PST_REBORN;
		memset (players[i].frags,0,sizeof(players[i].frags));
	}

	P_SetupLevel (gameepisode, gamemap);
	displayplayer = consoleplayer;      // view the guy you are playing
	starttime = I_GetTime ();
	gameaction = ga_nothing;
	Z_CheckHeap ();

//
// clear cmd building stuff
//

	memset (gamekeydown, 0, sizeof(gamekeydown));
	joyxmove = joyymove = 0;
	mousex = mousey = 0;
	sendpause = sendsave = paused = false;
	memset (mousebuttons, 0, sizeof(mousebuttons));
	memset (joybuttons, 0, sizeof(joybuttons));
}


/*
===============================================================================
=
= G_Responder
=
= get info needed to make ticcmd_ts for the players
=
===============================================================================
*/

void G_Responder(event_t *ev)
{
	// allow spy mode changes even during the demo
	if(gamestate == GS_LEVEL && ev->type == ev_keydown
		&& ev->data1 == KEY_F12 && (singledemo || !deathmatch) )
	{
		// spy mode 
		do
		{
			displayplayer++;
			if(displayplayer == MAXPLAYERS)
			{
				displayplayer = 0;
			}
		} while(!playeringame[displayplayer]
			&& displayplayer != consoleplayer);
		return;
	}
    
	// any other key pops up menu if in demos
	if (gameaction == ga_nothing && !singledemo && 
		(demoplayback || gamestate == GS_DEMOSCREEN) 
		) 
	{ 
		if (ev->type == ev_keydown ||  
			(ev->type == ev_mouse && ev->data1) || 
			(ev->type == ev_joystick && ev->data1) ) 
		{ 
			M_StartControlPanel (); 
		} 
		return; 
	} 

	if(gamestate == GS_LEVEL)
	{
		if (HU_Responder(ev))
		{
			return;	// chat ate the event
		}
		ST_Responder(ev);	// status window never ate it
		if (AM_Responder(ev))
		{
			return;	// automap ate it
		}
	}

	if (gamestate == GS_FINALE) 
	{ 
		if (F_Responder(ev))
		{
			return;	// finale ate the event
		}
	} 

	switch(ev->type)
	{
		case ev_keydown:
			if(ev->data1 == KEY_PAUSE)
			{
				sendpause = true;
				return;
			}
			if(ev->data1 < NUMKEYS)
			{
				gamekeydown[ev->data1] = true;
			}
			return; // eat key down events

		case ev_keyup:
			if(ev->data1 < NUMKEYS)
			{
				gamekeydown[ev->data1] = false;
			}
			return; // always let key up events filter down

		case ev_mouse:
			mousebuttons[0] = ev->data1&1;
			mousebuttons[1] = ev->data1&2;
			mousebuttons[2] = ev->data1&4;
			mousex = ev->data2*(mouseSensitivity+5)/10;
			mousey = ev->data3*(mouseSensitivity+5)/10;
			return; // eat events

		case ev_joystick:
			joybuttons[0] = ev->data1&1;
			joybuttons[1] = ev->data1&2;
			joybuttons[2] = ev->data1&4;
			joybuttons[3] = ev->data1&8;
			joyxmove = ev->data2;
			joyymove = ev->data3;
			return; // eat events

		default:
			break;
	}
}

/*
===============================================================================
=
= G_Ticker
=
===============================================================================
*/

void G_Ticker (void)
{
	int32_t                     i, buf;
	ticcmd_t        *cmd;

//
// do player reborns if needed
//
	for (i=0 ; i<MAXPLAYERS ; i++)
		if (playeringame[i] && players[i].playerstate == PST_REBORN)
			G_DoReborn (i);

//
// do things to change the game state
//
	while (gameaction != ga_nothing)
	{
		switch (gameaction)
		{
		case ga_loadlevel:
			G_DoLoadLevel ();
			break;
		case ga_newgame:
			G_DoNewGame ();
			break;
		case ga_loadgame:
			G_DoLoadGame ();
			break;
		case ga_savegame:
			G_DoSaveGame ();
			break;
		case ga_playdemo:
			G_DoPlayDemo ();
			break;
		case ga_completed:
			G_DoCompleted ();
			break;
		case ga_victory:
			F_StartFinale();
			break;
		case ga_worlddone:
			G_DoWorldDone();
			break;
		case ga_screenshot:
			M_ScreenShot ();
			gameaction = ga_nothing;
			break;
		case ga_nothing:
			break;
		}
	}


//
// get commands, check consistancy, and build new consistancy check
//
	buf = (gametic/ticdup)%BACKUPTICS;

	for (i=0 ; i<MAXPLAYERS ; i++)
		if (playeringame[i])
		{
			cmd = &players[i].cmd;

			memcpy (cmd, &netcmds[i][buf], sizeof(ticcmd_t));

			if (demoplayback)
				G_ReadDemoTiccmd (cmd);
			if (demorecording)
				G_WriteDemoTiccmd (cmd);
	    
			//
			// check for turbo cheats
			//
			if (cmd->forwardmove > TURBOTHRESHOLD && !(gametic&31) && ((gametic>>5)&3) == i )
			{
				static char turbomessage[80];
				extern char *player_names[4];
				sprintf (turbomessage, "%s is turbo!",player_names[i]);
				players[consoleplayer].message = turbomessage;
			}

			if (netgame && !netdemo && !(gametic%ticdup) )
			{
				if (gametic > BACKUPTICS
				&& consistancy[i][buf] != cmd->consistancy)
				{
					I_Error ("consistency failure (%i should be %i)",cmd->consistancy, consistancy[i][buf]);
				}
				if (players[i].mo)
					consistancy[i][buf] = players[i].mo->x;
				else
					consistancy[i][buf] = rndindex;
			}
		}

//
// check for special buttons
//
	for (i=0 ; i<MAXPLAYERS ; i++)
		if (playeringame[i])
		{
			if (players[i].cmd.buttons & BT_SPECIAL)
			{
				switch (players[i].cmd.buttons & BT_SPECIALMASK)
				{
				case BTS_PAUSE:
					paused ^= 1;
					if(paused)
					{
						S_PauseSound();
					}
					else
					{
						S_ResumeSound();
					}
					break;

				case BTS_SAVEGAME:
					if (!savedescription[0])
					{
						strcpy (savedescription, "NET GAME");
					}
					savegameslot =
						(players[i].cmd.buttons & BTS_SAVEMASK)>>BTS_SAVESHIFT;
					gameaction = ga_savegame;
					break;
				}
			}
		}
//
// do main actions
//
	switch (gamestate)
	{
		case GS_LEVEL:
			P_Ticker ();
			ST_Ticker ();
			AM_Ticker ();
			HU_Ticker ();
			break;
		case GS_INTERMISSION:
			WI_Ticker ();
			break;
		case GS_FINALE:
			F_Ticker ();
			break;
		case GS_DEMOSCREEN:
			D_PageTicker ();
			break;
	}
}


/*
==============================================================================

						PLAYER STRUCTURE FUNCTIONS

also see P_SpawnPlayer in P_Things
==============================================================================
*/

/*
====================
=
= G_PlayerFinishLevel
=
= Can when a player completes a level
====================
*/

static void G_PlayerFinishLevel(int32_t player)
{
	player_t *p;

	p = &players[player];

	memset(p->powers, 0, sizeof(p->powers));
	memset(p->cards, 0, sizeof(p->cards));
	p->mo->flags &= ~MF_SHADOW; // cancel invisibility
	p->extralight = 0; // cancel gun flashes
	p->fixedcolormap = 0; // cancel ir gogles
	p->damagecount = 0; // no palette changes
	p->bonuscount = 0;
}

/*
====================
=
= G_PlayerReborn
=
= Called after a player dies
= almost everything is cleared and initialized
====================
*/

void G_PlayerReborn(int32_t player)
{
	player_t *p;
	int32_t i;
	int32_t frags[MAXPLAYERS];
	int32_t killcount, itemcount, secretcount;

	memcpy(frags, players[player].frags, sizeof(frags));
	killcount = players[player].killcount;
	itemcount = players[player].itemcount;
	secretcount = players[player].secretcount;

	p = &players[player];
	memset(p, 0, sizeof(*p));

	memcpy(players[player].frags, frags, sizeof(players[player].frags));
	players[player].killcount = killcount;
	players[player].itemcount = itemcount;
	players[player].secretcount = secretcount;

	p->usedown = p->attackdown = true; // don't do anything immediately
	p->playerstate = PST_LIVE;
	p->health = MAXHEALTH;
	p->readyweapon = p->pendingweapon = wp_pistol;
	p->weaponowned[wp_fist] = true;
	p->weaponowned[wp_pistol] = true;
	p->ammo[am_clip] = 50;
	for(i = 0; i < NUMAMMO; i++)
	{
		p->maxammo[i] = maxammo[i];
	}
}

/*
====================
=
= G_CheckSpot
=
= Returns false if the player cannot be respawned at the given mapthing_t spot
= because something is occupying it
====================
*/

void P_SpawnPlayer (mapthing_t *mthing);

static boolean G_CheckSpot (int32_t playernum, mapthing_t *mthing)
{
	fixed_t         x,y;
	subsector_t *ss;
	uint32_t        an;
	mobj_t      *mo;
	int32_t         i;
	
	if (!players[playernum].mo)
	{
		// first spawn of level, before corpses
		for (i=0 ; i<playernum ; i++)
			if (players[i].mo->x == mthing->x << FRACBITS
				&& players[i].mo->y == mthing->y << FRACBITS)
				return false;	
		return true;
	}

	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;

	if (!P_CheckPosition (players[playernum].mo, x, y) )
		return false;
 
// flush an old corpse if needed 
	if (bodyqueslot >= BODYQUESIZE) 
		P_RemoveMobj (bodyque[bodyqueslot%BODYQUESIZE]); 
	bodyque[bodyqueslot%BODYQUESIZE] = players[playernum].mo; 
	bodyqueslot++; 

// spawn a teleport fog
	ss = R_PointInSubsector (x,y);
	an = ( ANG45 * (mthing->angle/45) ) >> ANGLETOFINESHIFT;

	mo = P_SpawnMobj (x+20*finecosine[an], y+20*finesine[an]
	, ss->sector->floorheight, MT_TFOG);

	if (players[consoleplayer].viewz != 1)
		S_StartSound (mo, sfx_telept);  // don't start sound on first frame

	return true;
}

/*
====================
=
= G_DeathMatchSpawnPlayer
=
= Spawns a player at one of the random death match spots
= called at level load and each death
====================
*/

void G_DeathMatchSpawnPlayer (int32_t playernum)
{
	int32_t             i,j;
	int32_t             selections;

	selections = deathmatch_p - deathmatchstarts;
	if (selections < 4)
		I_Error ("Only %i deathmatch spots, 4 required", selections);

	for (j=0 ; j<20 ; j++)
	{
		i = P_Random() % selections;
		if (G_CheckSpot (playernum, &deathmatchstarts[i]) )
		{
			deathmatchstarts[i].type = playernum+1;
			P_SpawnPlayer (&deathmatchstarts[i]);
			return;
		}
	}

// no good spot, so the player will probably get stuck
	P_SpawnPlayer (&playerstarts[playernum]);
}

/*
====================
=
= G_DoReborn
=
====================
*/

static void G_DoReborn (int32_t playernum)
{
	int32_t                             i;

	if (!netgame)
		gameaction = ga_loadlevel;                      // reload the level from scratch
	else
	{       // respawn at the start
		players[playernum].mo->player = NULL;   // dissasociate the corpse

		// spawn at random spot if in death match
		if (deathmatch)
		{
			G_DeathMatchSpawnPlayer (playernum);
			return;
		}

		if (G_CheckSpot (playernum, &playerstarts[playernum]) )
		{
			P_SpawnPlayer (&playerstarts[playernum]);
			return;
		}
		// try to spawn at one of the other players spots
		for (i=0 ; i<MAXPLAYERS ; i++)
			if (G_CheckSpot (playernum, &playerstarts[i]) )
			{
				playerstarts[i].type = playernum+1;             // fake as other player
				P_SpawnPlayer (&playerstarts[i]);
				playerstarts[i].type = i+1;                             // restore
				return;
			}
		// he's going to be inside something.  Too bad.
		P_SpawnPlayer (&playerstarts[playernum]);
	}
}


void G_ScreenShot (void)
{
	gameaction = ga_screenshot;
}



// DOOM Par Times
static int32_t pars[4][10] =
{
	{0},
#if APPVER_CHEX
	{0,120,360,480,200,360,180,180,30,165},
#else
	{0,30,75,120,90,165,180,180,30,165},
#endif
	{0,90,90,90,120,90,360,240,30,170},
	{0,90,45,90,150,90,90,165,30,135}
};

// DOOM II Par Times
static int32_t cpars[32] =
{
	30,90,120,120,90,150,120,120,270,90,	//  1-10
	210,150,150,150,210,150,420,150,210,150,	// 11-20
	240,150,180,150,150,300,330,420,300,180,	// 21-30
	120,30					// 31-32
};


/*
====================
=
= G_DoCompleted
=
====================
*/

static boolean         secretexit;
extern char     *pagename;

void G_ExitLevel (void)
{
	secretexit = false;
	gameaction = ga_completed;
}

// Here's for the german edition
void G_SecretExitLevel (void)
{
#if (APPVER_DOOMREV >= AV_DR_DM1666E)
	// IF NO WOLF3D LEVELS, NO SECRET EXIT!
	if (commercial && (W_CheckNumForName("map31")<0))
		secretexit = false;
	else
#endif
		secretexit = true; 
	gameaction = ga_completed;
}

static void G_DoCompleted(void)
{
	int32_t i;

	gameaction = ga_nothing;

	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(playeringame[i])
		{
			G_PlayerFinishLevel(i);        // take away cards and stuff 
		}
	}

	if (automapactive)
		AM_Stop();

	if (!commercial)
	{
		switch (gamemap)
		{
#if APPVER_CHEX
			case 5:
#else
			case 8:
#endif
				gameaction = ga_victory;
				return;
			case 9:
				for (i = 0; i < MAXPLAYERS; i++)
					players[i].didsecret = true;
				break;
		}
	}

	wminfo.didsecret = players[consoleplayer].didsecret;
	wminfo.epsd = gameepisode - 1;
	wminfo.last = gamemap - 1;

// wminfo.next is 0 biased, unlike gamemap
	if (commercial)
	{
		if (secretexit)
			switch (gamemap)
			{
				case 15: wminfo.next = 30; break;
				case 31: wminfo.next = 31; break;
			}
		else
			switch (gamemap)
			{
				case 31:
				case 32: wminfo.next = 15; break;
				default: wminfo.next = gamemap;
			}
	}
	else
	{
		if (secretexit)
			wminfo.next = 8; 	// go to secret level 
		else if (gamemap == 9)
		{
			// returning from secret level 
			switch (gameepisode)
			{
				case 1:
					wminfo.next = 3;
					break;
				case 2:
					wminfo.next = 5;
					break;
				case 3:
					wminfo.next = 6;
					break;
#if (APPVER_DOOMREV >= AV_DR_DM19U)
				case 4:
					wminfo.next = 2;
					break;
#endif
			}
		}
		else
			wminfo.next = gamemap;          // go to next level 
	}

	wminfo.maxkills = totalkills;
	wminfo.maxitems = totalitems;
	wminfo.maxsecret = totalsecret;
	wminfo.maxfrags = 0;
	if (commercial)
		wminfo.partime = TICRATE*cpars[gamemap-1];
	else
		wminfo.partime = TICRATE*pars[gameepisode][gamemap];
	wminfo.pnum = consoleplayer;

	for (i=0 ; i<MAXPLAYERS ; i++) 
	{
		wminfo.plyr[i].in = playeringame[i];
		wminfo.plyr[i].skills = players[i].killcount;
		wminfo.plyr[i].sitems = players[i].itemcount;
		wminfo.plyr[i].ssecret = players[i].secretcount;
		wminfo.plyr[i].stime = leveltime;
		memcpy (wminfo.plyr[i].frags, players[i].frags
				, sizeof(wminfo.plyr[i].frags));
	}

	gamestate = GS_INTERMISSION;
	viewactive = false;
	automapactive = false;

	if (statcopy)
		memcpy(statcopy, &wminfo, sizeof(wminfo));

	WI_Start(&wminfo);
}

//============================================================================
//
// G_WorldDone
//
//============================================================================

void G_WorldDone(void)
{
	gameaction = ga_worlddone;
	if (secretexit)
		players[consoleplayer].didsecret = true;

	if (commercial)
	{
		switch (gamemap)
		{
			case 15:
			case 31:
				if (!secretexit)
					break;
			case 6:
			case 11:
			case 20:
			case 30:
				F_StartFinale ();
				break;
		}
	}
}

static void G_DoWorldDone(void)
{
	gamestate = GS_LEVEL;
	gamemap = wminfo.next+1;
	G_DoLoadLevel();
	gameaction = ga_nothing;
	viewactive = true;
}

/*
====================
=
= G_InitFromSavegame
=
= Can be called by the startup code or the menu task. 
=
====================
*/

extern boolean setsizeneeded;
void R_ExecuteSetViewSize (void);

static char	savename[256];

void G_LoadGame (char* name) 
{ 
	strcpy (savename, name); 
	gameaction = ga_loadgame; 
}

#define VERSIONSIZE		16

static void G_DoLoadGame(void)
{
	int32_t i;
	int32_t a, b, c;
	char vcheck[VERSIONSIZE];

	gameaction = ga_nothing;

	M_ReadFile(savename, &savebuffer);
	save_p = savebuffer+SAVESTRINGSIZE;
	// Skip the description field
	memset(vcheck, 0, sizeof(vcheck));
	sprintf(vcheck, "version %i", VERSION);
	if (strcmp ((char const *)save_p, vcheck))
	{ // Bad version
		return;
	}
	save_p += VERSIONSIZE;
	gameskill = *save_p++;
	gameepisode = *save_p++;
	gamemap = *save_p++;
	for(i = 0; i < MAXPLAYERS; i++)
	{
		playeringame[i] = *save_p++;
	}
	// Load a base level
	G_InitNew(gameskill, gameepisode, gamemap);

	// Get the times
	a = *save_p++;
	b = *save_p++;
	c = *save_p++;
	leveltime = (a<<16)+(b<<8)+c;

	// De-archive all the modifications
	P_UnArchivePlayers();
	P_UnArchiveWorld();
	P_UnArchiveThinkers();
	P_UnArchiveSpecials();

	if(*save_p != 0x1d)
	{
		I_Error("Bad savegame");
	}
	Z_Free(savebuffer);

	if (setsizeneeded)
		R_ExecuteSetViewSize ();

	// Draw the pattern into the back screen
	R_FillBackScreen();
}


//==========================================================================
//
// G_SaveGame
//
// Called by the menu task.  <description> is a 24 byte text string.
//
//==========================================================================

void G_SaveGame(int32_t slot, char *description)
{
	savegameslot = slot;
	strcpy(savedescription, description);
	sendsave = true;
}

//==========================================================================
//
// G_DoSaveGame
//
// Called by G_Ticker based on gameaction.
//
//==========================================================================

static void G_DoSaveGame(void)
{
	char name[100];
	char name2[VERSIONSIZE];
	char *description;
	int32_t length;
	int32_t i;

#if (APPVER_DOOMREV >= AV_DR_DM1666E)
	if (M_CheckParm("-cdrom"))
	{
		sprintf(name, "c:\\doomdata\\"SAVEGAMENAME"%d.dsg", savegameslot);
	}
	else
#endif
	{
		sprintf(name, SAVEGAMENAME"%d.dsg", savegameslot);
	}
	description = savedescription;

	save_p = savebuffer = Z_Malloc(SAVEGAMESIZE, PU_STATIC, NULL);

	memcpy(save_p, description, SAVESTRINGSIZE);
	save_p += SAVESTRINGSIZE;
	memset (name2,0,sizeof(name2));
	sprintf (name2,"version %i",VERSION);
	memcpy (save_p, name2, VERSIONSIZE);
	save_p += VERSIONSIZE;

	*save_p++ = gameskill;
	*save_p++ = gameepisode;
	*save_p++ = gamemap;
	for(i = 0; i < MAXPLAYERS; i++)
	{
		*save_p++ = playeringame[i];
	}
	*save_p++ = leveltime>>16;
	*save_p++ = leveltime>>8;
	*save_p++ = leveltime;

	P_ArchivePlayers();
	P_ArchiveWorld();
	P_ArchiveThinkers();
	P_ArchiveSpecials();
	
	*save_p++ = 0x1d;		// consistancy marker
	 
	length = save_p - savebuffer;
	if (length > SAVEGAMESIZE)
		I_Error ("Savegame buffer overrun");
	M_WriteFile (name, savebuffer, length);
	Z_Free(savebuffer);
	gameaction = ga_nothing;
	savedescription[0] = 0;

	players[consoleplayer].message = GGSAVED;

	// Draw the pattern into the back screen
	R_FillBackScreen();
}


/*
====================
=
= G_InitNew
=
= Can be called by the startup code or the menu task
= consoleplayer, displayplayer, playeringame[] should be set
====================
*/

static skill_t d_skill;
static int32_t     d_episode;
static int32_t     d_map;

void G_DeferedInitNew (skill_t skill, int32_t episode, int32_t map)
{
	d_skill = skill;
	d_episode = episode;
	d_map = map;
	gameaction = ga_newgame;
}

static void G_DoNewGame (void)
{
	demoplayback = false;
	netdemo = false;
	netgame = false;
	deathmatch = false;
	playeringame[1] = playeringame[2] = playeringame[3] = 0;
	respawnparm = false;
	fastparm = false;
	nomonsters = false;
	consoleplayer = 0;
	G_InitNew (d_skill, d_episode, d_map);
	gameaction = ga_nothing;
}

extern  int32_t                     skytexture;

void G_InitNew(skill_t skill, int32_t episode, int32_t map)
{
	int32_t i;

	if(paused)
	{
		paused = false;
		S_ResumeSound();
	}


	if(skill > sk_nightmare)
		skill = sk_nightmare;
#if (APPVER_DOOMREV < AV_DR_DM19UP)
	if(episode < 1)
		episode = 1;
	if(episode > 3)
		episode = 3;
#else
	if(episode == 0)
		episode = 4;
#endif
	if(episode > 1 && shareware)
		episode = 1;
	if(map < 1)
		map = 1;
	if(map > 9 && !commercial)
		map = 9;
	M_ClearRandom();
	if(skill == sk_nightmare || respawnparm)
	{
		respawnmonsters = true;
	}
	else
	{
		respawnmonsters = false;
	}

	if (fastparm || (skill == sk_nightmare && gameskill != sk_nightmare))
	{
		for (i=S_SARG_RUN1 ; i<=S_SARG_PAIN2 ; i++)
			states[i].tics >>= 1;
		mobjinfo[MT_BRUISERSHOT].speed = 20*FRACUNIT;
		mobjinfo[MT_HEADSHOT].speed = 20*FRACUNIT;
		mobjinfo[MT_TROOPSHOT].speed = 20*FRACUNIT;
	}
	else if (skill != sk_nightmare && gameskill == sk_nightmare)
	{
		for (i=S_SARG_RUN1 ; i<=S_SARG_PAIN2 ; i++)
			states[i].tics <<= 1;
		mobjinfo[MT_BRUISERSHOT].speed = 15*FRACUNIT;
		mobjinfo[MT_HEADSHOT].speed = 10*FRACUNIT;
		mobjinfo[MT_TROOPSHOT].speed = 10*FRACUNIT;
	}


	// Force players to be initialized upon first level load
	for(i = 0; i < MAXPLAYERS; i++)
	{
		players[i].playerstate = PST_REBORN;
	}

	usergame = true; // will be set false if a demo
	paused = false;
	demoplayback = false;
	automapactive = false;
	viewactive = true;
	gameepisode = episode;
	gamemap = map;
	gameskill = skill;

	viewactive = true;

	// Set the sky map for the episode
	if (commercial)
	{
		skytexture = R_TextureNumForName("SKY3");
		if (gamemap < 12)
			skytexture = R_TextureNumForName("SKY1");
		else
			if (gamemap < 21)
				skytexture = R_TextureNumForName("SKY2");
	}
	else
		switch (episode)
		{
			case 1:
				skytexture = R_TextureNumForName("SKY1");
				break;
			case 2:
				skytexture = R_TextureNumForName("SKY2");
				break;
			case 3:
				skytexture = R_TextureNumForName("SKY3");
				break;
#if (APPVER_DOOMREV >= AV_DR_DM19UP)
			case 4:	// Special Edition sky
#if (APPVER_DOOMREV < AV_DR_DM19U)
				skytexture = R_TextureNumForName("SKY2");
#else
				skytexture = R_TextureNumForName("SKY4");
#endif
				break;
#endif
		}

	G_DoLoadLevel();
}


/*
===============================================================================

							DEMO RECORDING

===============================================================================
*/

#define DEMOMARKER      0x80

static void G_ReadDemoTiccmd (ticcmd_t *cmd)
{
	if (*demo_p == DEMOMARKER)
	{       // end of demo data stream
		G_CheckDemoStatus ();
		return;
	}
	cmd->forwardmove = ((int8_t)*demo_p++);
	cmd->sidemove = ((int8_t)*demo_p++);
	cmd->angleturn = ((uint8_t)*demo_p++)<<8;
	cmd->buttons = (uint8_t)*demo_p++;
}

static void G_WriteDemoTiccmd (ticcmd_t *cmd)
{
	if (gamekeydown['q'])           // press q to end demo recording
		G_CheckDemoStatus ();
	*demo_p++ = cmd->forwardmove;
	*demo_p++ = cmd->sidemove;
	*demo_p++ = (cmd->angleturn+128)>>8;
	*demo_p++ = cmd->buttons;
	demo_p -= 4;
	if (demo_p > demoend - 16)
	{
		// no more space 
		G_CheckDemoStatus();
		return;
	}

	G_ReadDemoTiccmd (cmd);         // make SURE it is exactly the same
}



/*
===================
=
= G_RecordDemo
=
===================
*/

void G_RecordDemo (char *name)
{
	int32_t             i;
	int32_t             maxsize;

	usergame = false;
	strcpy (demoname, name);
	strcat (demoname, ".lmp");
	maxsize = 0x20000;
	i = M_CheckParm ("-maxdemo");
	if (i && i<myargc-1)
		maxsize = atoi(myargv[i+1])*1024;
	demobuffer = Z_Malloc (maxsize,PU_STATIC,NULL);
	demoend = demobuffer + maxsize;

	demorecording = true;
}


void G_BeginRecording(void)
{
	int32_t             i;

	demo_p = demobuffer;

	*demo_p++ = VERSION;
	*demo_p++ = gameskill;
	*demo_p++ = gameepisode;
	*demo_p++ = gamemap;
	*demo_p++ = deathmatch;
	*demo_p++ = respawnparm;
	*demo_p++ = fastparm;
	*demo_p++ = nomonsters;
	*demo_p++ = consoleplayer;

	for (i=0 ; i<MAXPLAYERS; i++)
		*demo_p++ = playeringame[i];
}


/*
===================
=
= G_PlayDemo
=
===================
*/

static char    *defdemoname;

void G_DeferedPlayDemo (char *name)
{
	defdemoname = name;
	gameaction = ga_playdemo;
}

static void G_DoPlayDemo (void)
{
	skill_t skill;
	int32_t             i, episode, map;

	gameaction = ga_nothing;
	demobuffer = demo_p = W_CacheLumpName (defdemoname, PU_STATIC);
	if (*demo_p++ != VERSION)
		I_Error("Demo is from a different game version!");

	skill = *demo_p++;
	episode = *demo_p++;
	map = *demo_p++;
	deathmatch = *demo_p++;
	respawnparm = *demo_p++;
	fastparm = *demo_p++;
	nomonsters = *demo_p++;
	consoleplayer = *demo_p++;

	for (i=0 ; i<MAXPLAYERS ; i++)
		playeringame[i] = *demo_p++;
	if (playeringame[1])
	{
		netgame = true;
		netdemo = true;
	}

	precache = false;               // don't spend a lot of time in loadlevel
	G_InitNew (skill, episode, map);
	precache = true;
	usergame = false;
	demoplayback = true;
}


/*
===================
=
= G_TimeDemo
=
===================
*/

void G_TimeDemo (char *name)
{
	nodrawers = M_CheckParm ("-nodraw");
	noblit = M_CheckParm ("-noblit");
	timingdemo = true;
	singletics = true;

	defdemoname = name;
	gameaction = ga_playdemo;
}


/*
===================
=
= G_CheckDemoStatus
=
= Called after a death or level completion to allow demos to be cleaned up
===================
*/

void G_CheckDemoStatus (void)
{
	if (timingdemo)
	{
		int32_t realtics = I_GetTime() - starttime;
		int32_t resultfps = TICRATE * 1000 * gametic / realtics;
		I_Error ("Timed %i gametics in %i realtics. FPS: %u.%.3u", gametic, realtics, resultfps / 1000, resultfps % 1000);
	}

	if (demoplayback)
	{
		if (singledemo)
			I_Quit ();

		Z_ChangeTag (demobuffer, PU_CACHE);
		demoplayback = false;
		netdemo = false;
		netgame = false;
		deathmatch = false;
		playeringame[1] = playeringame[2] = playeringame[3] = 0;
		respawnparm = false;
		fastparm = false;
		nomonsters = false;
		consoleplayer = 0;
		D_AdvanceDemo ();
		return;
	}

	if (demorecording)
	{
		*demo_p++ = DEMOMARKER;
		M_WriteFile (demoname, demobuffer, demo_p - demobuffer);
		Z_Free (demobuffer);
		demorecording = false;
		I_Error ("Demo %s recorded",demoname);
	}

	return;
}


