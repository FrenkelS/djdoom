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

// D_main.c

#include <dos.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "DoomDef.h"
#include "soundst.h"
#include "DUtils.h"

#if (APPVER_DOOMREV < AV_DR_DM19)
#define	BGCOLOR		4
#define	FGCOLOR		15
#elif (APPVER_DOOMREV < AV_DR_DM19UP)
#define	BGCOLOR		7
#define	FGCOLOR		4
#elif (APPVER_DOOMREV < AV_DR_DM19U)
#define	BGCOLOR		3
#define	FGCOLOR		0
#else
#define	BGCOLOR		7
#define	FGCOLOR		8
#endif

#define MAXWADFILES 20
static char *wadfiles[MAXWADFILES];

boolean shareware;		// true if only episode 1 present
boolean registered;
boolean commercial;
#if (APPVER_DOOMREV >= AV_DR_DM18FR)
boolean french;
#if (APPVER_DOOMREV >= AV_DR_DM19F)
boolean plutonia;
boolean tnt;
#endif
#endif

boolean devparm;            // started game with -devparm
boolean nomonsters;			// checkparm of -nomonsters
boolean respawnparm;			// checkparm of -respawn
boolean fastparm;				// checkparm of -fastparm

boolean modifiedgame;

boolean singletics = false; // debug flag to cancel adaptiveness



extern int soundVolume;
extern  int	sfxVolume;
extern  int	musicVolume;

extern  boolean	inhelpscreens;

skill_t startskill;
int startepisode;
int startmap;
boolean autostart;

FILE *debugfile;

boolean advancedemo;




char basedefault[1024]; // default file


void D_CheckNetGame(void);
void D_ProcessEvents(void);
void G_BuildTiccmd(ticcmd_t *cmd);
void D_DoAdvanceDemo(void);
static void D_PageDrawer (void);
void D_AdvanceDemo (void);
void F_Drawer(void);
boolean F_Responder(event_t *ev);

/*
===============================================================================

							EVENT HANDLING

Events are asyncronous inputs generally generated by the game user.

Events can be discarded if no responder claims them

===============================================================================
*/

event_t events[MAXEVENTS];
int eventhead;
int eventtail;

/*
================
=
= D_PostEvent
=
= Called by the I/O functions when input is detected
=
================
*/

void D_PostEvent (event_t *ev)
{
	int temp;

	events[eventhead] = *ev;
	temp = (++eventhead)&(MAXEVENTS-1);
	eventhead = temp;
}

/*
================
=
= D_ProcessEvents
=
= Send all the events of the given timestamp down the responder chain
=
================
*/

void D_ProcessEvents (void)
{
	int temp;
	event_t		*ev;

#if (APPVER_DOOMREV >= AV_DR_DM1666E)
	// IF STORE DEMO, DO NOT ACCEPT INPUT
	if ( commercial && (W_CheckNumForName("map01")<0) )
		return;
#endif

	for ( ; eventtail != eventhead ; temp = (++eventtail)&(MAXEVENTS-1), eventtail = temp )
	{
		ev = &events[eventtail];
		if (M_Responder(ev))
			continue;               // menu ate the event
		G_Responder(ev);
	}
}

/*
================
=
= FixedMul
=
================
*/
fixed_t	FixedMul (fixed_t a, fixed_t b)
{
	return ((long long) a * (long long) b) >> FRACBITS;
}

/*
================
=
= FixedDiv
=
================
*/

fixed_t FixedDiv (fixed_t a, fixed_t b)
{
	if ( (abs(a)>>14) >= abs(b))
		return (a^b)<0 ? MININT : MAXINT;
	else
	{
		long long result = ((long long) a << FRACBITS) / b;
		return (fixed_t) result;
	}
}

/*
================
=
= D_Display
=
= draw current display, possibly wiping it from the previous
=
================
*/

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t wipegamestate = GS_DEMOSCREEN;
extern boolean setsizeneeded;
extern int showMessages;
void R_ExecuteSetViewSize (void);

static void D_Display (void)
{
	static boolean viewactivestate = false;
	static boolean menuactivestate = false;
	static boolean inhelpscreensstate = false;
	static boolean fullscreen = false;
	static gamestate_t oldgamestate = -1;
	static int borderdrawcount;
	int nowtime;
	int tics;
	int wipestart;
	int y;
	boolean done;
	boolean wipe;
	boolean redrawsbar;

	if (nodrawers)
		return;                    // for comparative timing / profiling

	redrawsbar = false;

	// Change the view size if needed
	if (setsizeneeded)
	{
		R_ExecuteSetViewSize ();
		oldgamestate = -1;                      // force background redraw
		borderdrawcount = 3;
	}

	// save the current screen if about to wipe
	if (gamestate != wipegamestate)
	{
		wipe = true;
		wipe_StartScreen();
	}
	else
		wipe = false;

	if (gamestate == GS_LEVEL && gametic)
		HU_Erase();

//
// do buffered drawing
//
	switch (gamestate)
	{
	case GS_LEVEL:
		if (!gametic)
			break;
		if (automapactive)
			AM_Drawer ();
		if (wipe || (viewheight != SCREENHEIGHT && fullscreen) )
			redrawsbar = true;
		if (inhelpscreensstate && !inhelpscreens)
			redrawsbar = true;              // just put away the help screen
		ST_Drawer (viewheight == SCREENHEIGHT, redrawsbar );
		fullscreen = viewheight == SCREENHEIGHT;
		break;
	case GS_INTERMISSION:
		WI_Drawer  ();
		break;
	case GS_FINALE:
		F_Drawer ();
		break;
	case GS_DEMOSCREEN:
		D_PageDrawer ();
		break;
	}
    
	// draw buffered stuff to screen
	I_UpdateNoBlit ();
	
	// draw the view directly
	if (gamestate == GS_LEVEL && !automapactive && gametic)
		R_RenderPlayerView (&players[displayplayer]);
	
	if (gamestate == GS_LEVEL && gametic)
		HU_Drawer ();
	
	// clean up border stuff
	if (gamestate != oldgamestate && gamestate != GS_LEVEL)
		I_SetPalette (W_CacheLumpName ("PLAYPAL",PU_CACHE));
	
	// see if the border needs to be initially drawn
	if (gamestate == GS_LEVEL && oldgamestate != GS_LEVEL)
	{
		viewactivestate = false;        // view was not active
		R_FillBackScreen ();    // draw the pattern into the back screen
	}
	
	// see if the border needs to be updated to the screen
	if (gamestate == GS_LEVEL && !automapactive && scaledviewwidth != SCREENWIDTH)
	{
		if (menuactive || menuactivestate || !viewactivestate)
			borderdrawcount = 3;
		if (borderdrawcount)
		{
			R_DrawViewBorder ();    // erase old menu stuff
			borderdrawcount--;
		}
	
	}
	
	menuactivestate = menuactive;
	viewactivestate = viewactive;
	inhelpscreensstate = inhelpscreens;
	oldgamestate = wipegamestate = gamestate;
	
	// draw pause pic
	if (paused)
	{
		if (automapactive)
			y = 4;
		else
			y = viewwindowy+4;
		V_DrawPatchDirect(viewwindowx+(scaledviewwidth-68)/2,
						  y,0,W_CacheLumpName ("M_PAUSE", PU_CACHE));
	}
	
	
	// menus go directly to the screen
	M_Drawer ();          // menu is drawn even on top of everything
	NetUpdate ();         // send out any new accumulation
	
	
	// normal update
	if (!wipe)
	{
		I_FinishUpdate ();              // page flip or blit buffer
		return;
	}
	
	// wipe update
	wipe_EndScreen();
	
	wipestart = I_GetTime () - 1;
	
	do
	{
		do
		{
			nowtime = I_GetTime ();
			tics = nowtime - wipestart;
		} while (!tics);
		wipestart = nowtime;
		done = wipe_ScreenWipe(tics);
		I_UpdateNoBlit ();
		M_Drawer ();                            // menu is drawn even on top of wipes
		I_FinishUpdate ();                      // page flip or blit buffer
	} while (!done);
}

/*
================
=
= D_DoomLoop
=
================
*/

static void D_DoomLoop (void)
{
	if (demorecording)
		G_BeginRecording ();

	if (M_CheckParm ("-debugfile"))
	{
		char	filename[20];
		sprintf (filename, "debug%i.txt", consoleplayer);
		printf ("debug output to: %s\n", filename);
		debugfile = fopen (filename,"w");
	}
	I_InitGraphics ();
	while (1)
	{
		// frame syncronous IO operations
		I_StartFrame();

		// process one or more tics
		if (singletics)
		{
			I_StartTic ();
			D_ProcessEvents ();
			G_BuildTiccmd (&netcmds[consoleplayer][maketic%BACKUPTICS]);
			if (advancedemo)
				D_DoAdvanceDemo ();
			M_Ticker ();
			G_Ticker ();
			gametic++;
			maketic++;
		}
		else
		{
			// will run at least one tic
			TryRunTics ();
		}

		// move positional sounds
		S_UpdateSounds(players[consoleplayer].mo);
		D_Display();
	}
}

/*
===============================================================================

						DEMO LOOP

===============================================================================
*/

static int             demosequence;
static int             pagetic;
static char            *pagename;


/*
================
=
= D_PageTicker
=
= Handles timing for warped projection
=
================
*/

void D_PageTicker (void)
{
	if (--pagetic < 0)
		D_AdvanceDemo ();
}


/*
================
=
= D_PageDrawer
=
================
*/

static void D_PageDrawer (void)
{
	V_DrawPatch (0,0, 0, W_CacheLumpName(pagename, PU_CACHE));
}

/*
=================
=
= D_AdvanceDemo
=
= Called after each demo or intro demosequence finishes
=================
*/

void D_AdvanceDemo (void)
{
	advancedemo = true;
}

void D_DoAdvanceDemo (void)
{
	players[consoleplayer].playerstate = PST_LIVE;  // don't reborn
	advancedemo = false;
	usergame = false;               // can't save / end game here
	paused = false;
	gameaction = ga_nothing;
#if (APPVER_DOOMREV >= AV_DR_DM19U) && (APPVER_DOOMREV < AV_DR_DM19F2)
	demosequence = (demosequence+1)%7;
#else
	demosequence = (demosequence+1)%6;
#endif
	switch (demosequence)
	{
		case 0:
			if ( commercial )
				pagetic = TICRATE * 11;
			else
				pagetic = 170;
			gamestate = GS_DEMOSCREEN;
			pagename = "TITLEPIC";
			if ( commercial )
				S_StartMusic(mus_dm2ttl);
			else
				S_StartMusic (mus_intro);
			break;
		case 1:
			G_DeferedPlayDemo ("demo1");
			break;
		case 2:
			pagetic = 200;
			gamestate = GS_DEMOSCREEN;
			pagename = "CREDIT";
			break;
		case 3:
			G_DeferedPlayDemo ("demo2");
			break;
		case 4:
			gamestate = GS_DEMOSCREEN;
			if ( commercial )
			{
				pagetic = TICRATE * 11;
				pagename = "TITLEPIC";
				S_StartMusic(mus_dm2ttl);
			}
			else
			{
				pagetic = 200;
#if (APPVER_DOOMREV < AV_DR_DM19U)
				pagename = "HELP2";
#else
				pagename = "CREDIT";
#endif
			}
			break;
		case 5:
			G_DeferedPlayDemo ("demo3");
			break;
#if (APPVER_DOOMREV >= AV_DR_DM19U)
			// THE DEFINITIVE DOOM Special Edition demo
		case 6:
			G_DeferedPlayDemo ("demo4");
			break;
#endif
	}
}


/*
=================
=
= D_StartTitle
=
=================
*/

void D_StartTitle (void)
{
	gameaction = ga_nothing;
	demosequence = -1;
	D_AdvanceDemo ();
}

static char title[128]; //      print title for every printed line

static int GetTextX(void)
{
	union REGS regs;
	regs.h.ah = 3;
	regs.h.bh = 0;
	int386(0x10, &regs, &regs);
	return regs.h.dl;
}

static int GetTextY(void)
{
	union REGS regs;
	regs.h.ah = 3;
	regs.h.bh = 0;
	int386(0x10, &regs, &regs);
	return regs.h.dh;
}

static void SetTextPos(int x, int y)
{
	union REGS regs;
	regs.h.ah = 2;
	regs.h.bh = 0;
	regs.h.dh = y;
	regs.h.dl = x;
	int386(0x10, &regs, &regs);
}

static void tprintf(char *msg, int fgcolor, int bgcolor)
{
	union REGS regs;
	byte attr;
	int x;
	int y;
	int i;

	attr = (bgcolor << 4) | fgcolor;
	x = GetTextX();
	y = GetTextY();

	for (i = 0; i < strlen(msg); i++)
	{
		regs.h.ah = 9;
		regs.h.al = msg[i];
		regs.w.cx = 1;
		regs.h.bl = attr;
		regs.h.bh = 0;
		int386(0x10, &regs, &regs);
		if (++x > 79)
			x = 0;

		SetTextPos(x, y);
	}
}

void mprintf(char *msg)
{
	int x, y;
	printf(msg);

	x = GetTextX();
	y = GetTextY();

	SetTextPos(0, 0);

	tprintf(title, FGCOLOR, BGCOLOR);

	SetTextPos(x, y);
}

/*
===============
=
= D_AddFile
=
===============
*/

static void D_AddFile(char *file)
{
	int numwadfiles;
	char *new;

	for(numwadfiles = 0; wadfiles[numwadfiles]; numwadfiles++);
	new = malloc(strlen(file)+1);
	strcpy(new, file);

	wadfiles[numwadfiles] = new;
}

#if (APPVER_DOOMREV < AV_DR_DM19U)
#define DEVMAPS "e:/doom/"
#else
#define DEVMAPS "f:/doom/data_se/"
#endif
#define DEVDATA "c:/localid/"

/*
=================
=
= IdentifyVersion
=
= Checks availability of IWAD files by name,
= to determine whether registered/commercial features
= should be executed (notably loading PWAD's).
=================
*/
static void IdentifyVersion (void)
{

	char	*doom1wad, *doomwad, *doom2wad;
#if (APPVER_DOOMREV >= AV_DR_DM18FR)
#if (APPVER_DOOMREV < AV_DR_DM19F)
	char	*doom2fwad;
#else
	char	*doom2fwad, *plutoniawad, *tntwad;
#endif
#endif // AV_DR_DM18FR
	strcpy(basedefault,"default.cfg");
	doom1wad = "doom1.wad";
#if (APPVER_DOOMREV >= AV_DR_DM18FR)
	doom2fwad = "doom2f.wad";
#endif
	doom2wad = "doom2.wad";
#if (APPVER_DOOMREV >= AV_DR_DM19F)
	plutoniawad = "plutonia.wad";
	tntwad = "tnt.wad";
#endif
#if APPVER_CHEX
	doomwad = "chex.wad";
#else
	doomwad = "doom.wad";
#endif
	if (M_CheckParm ("-shdev"))
	{
		registered = false;
		shareware = true;
		devparm = true;
		D_AddFile (DEVDATA"doom1.wad");
#if (APPVER_DOOMREV < AV_DR_DM19U)
		D_AddFile (DEVMAPS"data/texture1.lmp");
		D_AddFile (DEVMAPS"data/pnames.lmp");
#else
		D_AddFile (DEVMAPS"data_se/texture1.lmp");
		D_AddFile (DEVMAPS"data_se/pnames.lmp");
#endif
		strcpy (basedefault,DEVDATA"default.cfg");
		return;
	}
	
	if (M_CheckParm ("-regdev"))
	{
		registered = true;
		devparm = true;
#if APPVER_CHEX
		D_AddFile (DEVDATA"chex.wad");
#else
		D_AddFile (DEVDATA"doom.wad");
#endif
#if (APPVER_DOOMREV < AV_DR_DM19U)
		D_AddFile (DEVMAPS"data/texture1.lmp");
		D_AddFile (DEVMAPS"data/texture2.lmp");
		D_AddFile (DEVMAPS"data/pnames.lmp");
#else
		D_AddFile (DEVMAPS"data_se/texture1.lmp");
		D_AddFile (DEVMAPS"data_se/texture2.lmp");
		D_AddFile (DEVMAPS"data_se/pnames.lmp");
#endif
		strcpy (basedefault,DEVDATA"default.cfg");
		return;
	}
	
	if (M_CheckParm ("-comdev"))
	{
		commercial = true;
		devparm = true;
#if (APPVER_DOOMREV >= AV_DR_DM19F)
		if(plutonia)
			D_AddFile (DEVDATA"plutonia.wad");
		else if(tnt)
			D_AddFile (DEVDATA"tnt.wad");
		else
#endif
			D_AddFile (DEVDATA"doom2.wad");
		    
		D_AddFile (DEVMAPS"cdata/texture1.lmp");
		D_AddFile (DEVMAPS"cdata/pnames.lmp");
		strcpy (basedefault,DEVDATA"default.cfg");
		return;
	}
	
#if (APPVER_DOOMREV >= AV_DR_DM18FR)
	if ( !access (doom2fwad,R_OK) )
	{
		commercial = true;
		// C'est ridicule!
		// Let's handle languages in config files, okay?
		french = true;
		printf("French version\n");
		D_AddFile (doom2fwad);
		return;
	}
#endif
	
	if ( !access (doom2wad,R_OK) )
	{
		commercial = true;
		D_AddFile (doom2wad);
		return;
	}
	
#if (APPVER_DOOMREV >= AV_DR_DM19F)
	if ( !access (plutoniawad, R_OK ) )
	{
		commercial = true;
		plutonia = true;
		D_AddFile (plutoniawad);
		return;
	}
	
	if ( !access (tntwad, R_OK ) )
	{
		commercial = true;
		tnt = true;
		D_AddFile (tntwad);
		return;
	}
#endif // APPVER_DOOMREV >= AV_DR_DM19F
	
	if ( !access (doomwad, R_OK ) )
	{
		registered = true;
		D_AddFile (doomwad);
		return;
	}
	
	if ( !access (doom1wad, R_OK ) )
	{
		shareware = true;
		D_AddFile (doom1wad);
		return;
	}
	
#if (APPVER_DOOMREV < AV_DR_DM18FR)
	I_Error("Game mode indeterminate\n");
#else
	printf("Game mode indeterminate.\n");
	exit(1);
#endif
}

/*
=================
=
= Find a Response File
=
=================
*/
static void FindResponseFile (void)
{
	int i;
#define MAXARGVS 100
	
	for (i = 1;i < myargc;i++)
		if (myargv[i][0] == '@')
		{
			FILE *handle;
			int size;
			int k;
			int index;
			int indexinfile;
			char *infile;
			char *file;
			char *moreargs[20];
			char *firstargv;
		
			// READ THE RESPONSE FILE INTO MEMORY
			handle = fopen (&myargv[i][1],"rb");
			if (!handle)
			{
#if (APPVER_DOOMREV < AV_DR_DM1666)
				I_Error ("No such response file!");
#else
				printf ("\nNo such response file!");
				exit(1);
#endif
			}
			printf("Found response file %s!\n",&myargv[i][1]);
			fseek (handle,0,SEEK_END);
			size = ftell(handle);
			fseek (handle,0,SEEK_SET);
			file = malloc (size);
			fread (file,size,1,handle);
			fclose (handle);
		
			// KEEP ALL CMDLINE ARGS FOLLOWING @RESPONSEFILE ARG
			for (index = 0,k = i+1; k < myargc; k++)
				moreargs[index++] = myargv[k];
		
			firstargv = myargv[0];
			myargv = malloc(sizeof(char *)*MAXARGVS);
			memset(myargv,0,sizeof(char *)*MAXARGVS);
			myargv[0] = firstargv;
		
			infile = file;
			indexinfile = k = 0;
			indexinfile++;  // SKIP PAST ARGV[0] (KEEP IT)
			do
			{
				myargv[indexinfile++] = infile+k;
				while(k < size &&
					((*(infile+k)>= ' '+1) && (*(infile+k)<='z')))
					k++;
				*(infile+k) = 0;
				while(k < size &&
					((*(infile+k)<= ' ') || (*(infile+k)>'z')))
					k++;
			} while(k < size);
		
			for (k = 0;k < index;k++)
				myargv[indexinfile++] = moreargs[k];
			myargc = indexinfile;
	
			// DISPLAY ARGS
			printf("%d command-line args:\n",myargc);
			for (k=1;k<myargc;k++)
				printf("%s\n",myargv[k]);
	
			break;
		}
}

/*
=================
=
= D_DoomMain
=
=================
*/

void D_DoomMain (void)
{
	union REGS regs;
	int p;
	char file[256];

	FindResponseFile ();

	IdentifyVersion ();
	
	setbuf (stdout, NULL);
	modifiedgame = false;
	
	nomonsters = M_CheckParm ("-nomonsters");
	respawnparm = M_CheckParm ("-respawn");
	fastparm = M_CheckParm ("-fast");
	devparm = M_CheckParm ("-devparm");
#if !APPVER_CHEX
	if (M_CheckParm ("-altdeath"))
		deathmatch = 2;
	else if (M_CheckParm ("-deathmatch"))
		deathmatch = 1;
#endif

	if (!commercial)
	{
		sprintf (title,
#if (APPVER_DOOMREV < AV_DR_DM17)
				 "                          "
				 "DOOM System Startup v%i.%i66"
				 "                          ",
#elif (APPVER_DOOMREV < AV_DR_DM19UP)
				 "                          "
				 "DOOM System Startup v%i.%i"
				 "                          ",
#elif (APPVER_DOOMREV < AV_DR_DM19U)
				 "                   "
				 "DOOM System Startup v%i.%i Special Edition"
				 "                          ",
#elif !APPVER_CHEX
				 "                         "
				 "The Ultimate DOOM Startup v%i.%i"
				 "                        ",
#else
				 "                         "
				 "Chex(R) Quest Startup"
				 "                        ",
#endif
				 VERSION/100,VERSION%100);
	}
#if (APPVER_DOOMREV >= AV_DR_DM19F)
	else if (plutonia)
	{
		sprintf (title,
				 "                   "
#if APPVER_CHEX
				 "Chex(R) Quest"
#else
				 "DOOM 2: Plutonia Experiment v%i.%i"
#endif
				 "                           ",
				 VERSION/100,VERSION%100);
	}
	else if (tnt)
	{
		sprintf (title,
				 "                     "
#if APPVER_CHEX
				 "Chex(R) Quest"
#else
				 "DOOM 2: TNT - Evilution v%i.%i"
#endif
				 "                           ",
				 VERSION/100,VERSION%100);
	}
#endif // APPVER_DOOMREV >= AV_DR_DM19F
	else
	{
		sprintf (title,
				 "                         "
#if (APPVER_DOOMREV < AV_DR_DM17)
				 "DOOM 2: Hell on Earth v%i.%i66"
				 "                          ",
#elif (APPVER_DOOMREV < AV_DR_DM17A)
				 "DOOM 2: Hell on Earth v%i.%i"
				 "                          ",
#elif (APPVER_DOOMREV < AV_DR_DM18FR)
				 "DOOM 2: Hell on Earth v%i.%ia"
				 "                          ",
#elif !APPVER_CHEX
				 "DOOM 2: Hell on Earth v%i.%i"
				 "                           ",
#else
				 "Chex(R) Quest"
				 "                           ",
#endif
				 VERSION/100,VERSION%100);
	}

	regs.w.ax = 3;
	int386 (0x10, &regs, &regs);

	tprintf (title, FGCOLOR, BGCOLOR);

#if (APPVER_DOOMREV < AV_DR_DM19)
	printf ("\n");
#else
	printf ("\nP_Init: Checking cmd-line parameters...\n");
#endif
	
	if (devparm)
		mprintf(D_DEVSTR);
	
	if (M_CheckParm("-cdrom"))
	{
		printf(D_CDROM);
#if (APPVER_DOOMREV < AV_DR_DM1666E)
		mkdir("c:doomdata");
#elif (APPVER_DOOMREV < AV_DR_DM17)
		mkdir("c:doomdata",0);
#else
#if defined __DJGPP__
		mkdir("c:\\doomdata",0);
#elif defined __WATCOMC__
		mkdir("c:\\doomdata");
#endif
#endif
		strcpy (basedefault,"c:/doomdata/default.cfg");
	}	
	
	// turbo option
	if ( (p=M_CheckParm ("-turbo")) )
	{
		int  scale = 200;
		extern int forwardmove[2];
		extern int sidemove[2];
	
		if (p<myargc-1)
			scale = atoi (myargv[p+1]);
		if (scale < 10)
			scale = 10;
		if (scale > 400)
			scale = 400;
		printf ("turbo scale: %i%%\n",scale);
		forwardmove[0] = forwardmove[0]*scale/100;
		forwardmove[1] = forwardmove[1]*scale/100;
		sidemove[0] = sidemove[0]*scale/100;
		sidemove[1] = sidemove[1]*scale/100;
	}
	
	// add any files specified on the command line with -file wadfile
	// to the wad list
	//
	// convenience hack to allow -wart e m to add a wad file
	// prepend a tilde to the filename so wadfile will be reloadable
	p = M_CheckParm ("-wart");
	if (p)
	{
		myargv[p][4] = 'p';     // big hack, change to -warp

		// Map name handling.
		if (commercial)
		{
			p = atoi (myargv[p+1]);
			if (p<10)
				sprintf (file,"~"DEVMAPS"cdata/map0%i.wad", p);
			else
				sprintf (file,"~"DEVMAPS"cdata/map%i.wad", p);
		}
		else
		{
			sprintf (file,"~"DEVMAPS"E%cM%c.wad",
				myargv[p+1][0], myargv[p+2][0]);
			printf("Warping to Episode %s, Map %s.\n",
				myargv[p+1],myargv[p+2]);
		}
		D_AddFile (file);
	}
	
	p = M_CheckParm ("-file");
	if (p)
	{
		// the parms after p are wadfile/lump names,
		// until end of parms or another - preceded parm
		modifiedgame = true;            // homebrew levels
		while (++p != myargc && myargv[p][0] != '-')
			D_AddFile (myargv[p]);
	}
	
	p = M_CheckParm ("-playdemo");
	
	if (!p)
		p = M_CheckParm ("-timedemo");
	
	if (p && p < myargc-1)
	{
		sprintf (file,"%s.lmp", myargv[p+1]);
		D_AddFile (file);
		printf("Playing demo %s.lmp.\n",myargv[p+1]);
	}
	
	// get skill / episode / map from parms
	startskill = sk_medium;
	startepisode = 1;
	startmap = 1;
	autostart = false;
	
		
	p = M_CheckParm ("-skill");
	if (p && p < myargc-1)
	{
		startskill = myargv[p+1][0]-'1';
		autostart = true;
	}
	
	p = M_CheckParm ("-episode");
	if (p && p < myargc-1)
	{
		startepisode = myargv[p+1][0]-'0';
		startmap = 1;
		autostart = true;
	}
	
	p = M_CheckParm ("-timer");
	if (p && p < myargc-1 && deathmatch)
	{
		int time;
		time = atoi(myargv[p+1]);
		printf("Levels will end after %d minute",time);
		if (time>1)
			printf("s");
		printf(".\n");
	}
	
	p = M_CheckParm ("-avg");
	if (p && p < myargc-1 && deathmatch)
		printf("Austin Virtual Gaming: Levels will end after 20 minutes\n");
	
	p = M_CheckParm ("-warp");
	if (p && p < myargc-1)
	{
		if (commercial)
			startmap = atoi (myargv[p+1]);
		else
		{
			startepisode = myargv[p+1][0]-'0';
			startmap = myargv[p+2][0]-'0';
		}
		autostart = true;
	}
	
	// init subsystems
	printf ("M_LoadDefaults: Load system defaults.\n");
	M_LoadDefaults ();              // load before initing other systems
	
	printf ("Z_Init: Init zone memory allocation daemon. \n");
	Z_Init ();
	
	printf ("V_Init: allocate screens.\n");
	V_Init ();
	
	printf ("W_Init: Init WADfiles.\n");
	W_InitMultipleFiles (wadfiles);
	
	
	// Check for -file in shareware
	if (modifiedgame)
	{
		// These are the lumps that will be checked in IWAD,
		// if any one is not present, execution will be aborted.
		char name[23][8]=
		{
			"e2m1","e2m2","e2m3","e2m4","e2m5","e2m6","e2m7","e2m8","e2m9",
			"e3m1","e3m3","e3m3","e3m4","e3m5","e3m6","e3m7","e3m8","e3m9",
			"dphoof","bfgga0","heada1","cybra1","spida1d1"
		};
		int i;
	
		// Check for fake IWAD with right name,
		// but w/o all the lumps of the registered version. 
		if (registered)
			for (i = 0;i < 23; i++)
				if (W_CheckNumForName(name[i])<0)
#if APPVER_CHEX
					I_Error("\nThis is Chex(R) Quest.");
#else
					I_Error("\nThis is not the registered version.");
#endif
	}
#if (APPVER_DOOMREV < AV_DR_DM1666E)
	if (commercial)
	{
		char name[5][8]=
		{
			"fatti5","map20","keena0","vileg1","skelg1"
		};
		int i;
		for (i = 0;i < 5; i++)
			if (W_CheckNumForName(name[i])<0)
				I_Error("\nThis is not the DOOM 2 wadfile.");
	}
#endif
	
#if !APPVER_CHEX
	// Iff additonal PWAD files are used, print modified banner
	if (modifiedgame)
	{
		/*m*/printf(
			"===========================================================================\n"
			"ATTENTION:  This version of DOOM has been modified.  If you would like to\n"
			"get a copy of the original game, call 1-800-IDGAMES or see the readme file.\n"
			"        You will not receive technical support for modified games.\n"
			"                      press enter to continue\n"
			"===========================================================================\n"
			);
		getchar ();
	}
#endif
	
	
	// Check and print which version is executed.
	if (registered)
	{
		mprintf("\tregistered version.\n");
#if !APPVER_CHEX
		mprintf (
			"===========================================================================\n"
			"             This version is NOT SHAREWARE, do not distribute!\n"
			"         Please report software piracy to the SPA: 1-800-388-PIR8\n"
			"===========================================================================\n"
		);
#endif
	}
	if (shareware)
	{
		mprintf("\tshareware version.\n");
	}
	if (commercial)
	{
		mprintf("\tcommercial version.\n");
#if !APPVER_CHEX
		mprintf (
			"===========================================================================\n"
			"                            Do not distribute!\n"
			"         Please report software piracy to the SPA: 1-800-388-PIR8\n"
			"===========================================================================\n"
		);
#endif
	}
	
	mprintf ("M_Init: Init miscellaneous info.\n");
	M_Init ();
	
#if APPVER_CHEX
	mprintf ("R_Init: Init Chex(R) Quest refresh daemon - ");
#else
	mprintf ("R_Init: Init DOOM refresh daemon - ");
#endif
	R_Init ();
	
	mprintf ("\nP_Init: Init Playloop state.\n");
	P_Init ();
	
	mprintf ("I_Init: Setting up machine state.\n");
	I_Init ();
	
	mprintf ("D_CheckNetGame: Checking network game status.\n");
	D_CheckNetGame ();
	
	mprintf ("S_Init: Setting up sound.\n");
	S_Init (sfxVolume*8, musicVolume*8);
	
	mprintf ("HU_Init: Setting up heads up display.\n");
	HU_Init ();
	
	mprintf ("ST_Init: Init status bar.\n");
	ST_Init ();
	
	// check for a driver that wants intermission stats
	p = M_CheckParm ("-statcopy");
	if (p && p<myargc-1)
	{
		// for statistics driver
		extern void* statcopy;                            
	
		statcopy = (void*)atoi(myargv[p+1]);
		mprintf ("External statistics registered.\n");
	}
	
	// start the apropriate game based on parms
	p = M_CheckParm ("-record");
	
	if (p && p < myargc-1)
	{
		G_RecordDemo (myargv[p+1]);
		autostart = true;
	}
	
	p = M_CheckParm ("-playdemo");
	if (p && p < myargc-1)
	{
		singledemo = true;              // quit after one demo
		G_DeferedPlayDemo (myargv[p+1]);
		D_DoomLoop ();  // never returns
	}
	
	p = M_CheckParm ("-timedemo");
	if (p && p < myargc-1)
	{
		G_TimeDemo (myargv[p+1]);
		D_DoomLoop ();  // never returns
	}
	
	p = M_CheckParm ("-loadgame");
	if (p && p < myargc-1)
	{
#if (APPVER_DOOMREV >= AV_DR_DM1666E)
		if (M_CheckParm("-cdrom"))
			sprintf(file, "c:\\doomdata\\"SAVEGAMENAME"%c.dsg",myargv[p+1][0]);
		else
#endif
			sprintf(file, SAVEGAMENAME"%c.dsg",myargv[p+1][0]);
		G_LoadGame (file);
	}
	
	
	if ( gameaction != ga_loadgame )
	{
		if (autostart || netgame)
			G_InitNew (startskill, startepisode, startmap);
		else
			D_StartTitle ();                // start up intro loop
	
	}
	
	D_DoomLoop ();  // never returns
}
