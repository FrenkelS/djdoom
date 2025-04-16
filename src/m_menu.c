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

// M_menu.c

#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include "doomdef.h"
#include "r_local.h"
#include "hu_stuff.h"
#include "soundst.h"

extern patch_t *hu_font[HU_FONTSIZE];
extern boolean message_dontfuckwithme;

extern boolean chat_on;		// in heads-up code

//
// defaulted values
//
int32_t mouseSensitivity;       // has default

// Show messages has default, 0 = off, 1 = on
int32_t showMessages;
	
int32_t sfxVolume;
int32_t musicVolume;

// Blocky mode, has default, 0 = high, 1 = normal
int32_t detailLevel;		
int32_t screenblocks;		// has default

// temp for screenblocks (0-9)
static int32_t screenSize;		

// -1 = no quicksave slot picked!
static int32_t quickSaveSlot;          

 // 1 = message to be printed
static int32_t messageToPrint;
// ...and here is the message string!
static char *messageString;		

// message x & y
static int32_t messageLastMenuActive;

// timed message = no input from user
static boolean messageNeedsInput;     

static void (*messageRoutine)(int32_t response);

static char gammamsg[5][26] =
{
	GAMMALVL0,
	GAMMALVL1,
	GAMMALVL2,
	GAMMALVL3,
	GAMMALVL4
};

static char endmsg[8][80] =
{
	QUITMSG,
#if APPVER_CHEX
	"please don't leave, we\nneed your help!",
	"please don't leave, we\nneed your help!",
	"please don't leave, we\nneed your help!",
	"please don't leave, we\nneed your help!",
	"please don't leave, we\nneed your help!",
	"please don't leave, we\nneed your help!",
	"please don't leave, we\nneed your help!"
#else
	"please don't leave, there's more\ndemons to toast!",
	"let's beat it -- this is turning\ninto a bloodbath!",
	"i wouldn't leave if i were you.\ndos is much worse.",
	"you're trying to say you like dos\nbetter than me, right?",
	"don't leave yet -- there's a\ndemon around that corner!",
	"ya know, next time you come in here\ni'm gonna toast ya.",
	"go ahead and leave. see if i care."
#endif
};

static char endmsg2[8][80] =
{
	QUITMSG,
#if APPVER_CHEX
	"please don't leave, we\nneed your help!",
	"please don't leave, we\nneed your help!",
	"please don't leave, we\nneed your help!",
	"please don't leave, we\nneed your help!",
	"please don't leave, we\nneed your help!",
	"please don't leave, we\nneed your help!",
	"please don't leave, we\nneed your help!"
#else
	"you want to quit?\nthen, thou hast lost an eighth!",
	"don't go now, there's a \ndimensional shambler waiting\nat the dos prompt!",
	"get outta here and go back\nto your boring programs.",
	"if i were your boss, i'd \n deathmatch ya in a minute!",
	"look, bud. you leave now\nand you forfeit your body count!",
	"just leave. when you come\nback, i'll be waiting with a bat.",
	"you're lucky i don't smack\nyou for thinking about leaving."
#endif
};

// we are going to be entering a savegame string
static int32_t saveStringEnter;              
static int32_t saveSlot; // which slot to save in
static int32_t saveCharIndex; // which char we're editing
// old save description before edit
static char saveOldString[SAVESTRINGSIZE];  

boolean inhelpscreens;
boolean menuactive;

#define SKULLXOFF -32
#define LINEHEIGHT 16

extern boolean sendpause;
static char savegamestrings[10][SAVESTRINGSIZE];

static char endstring[160];


//
// MENU TYPEDEFS
//
typedef struct
{
	int16_t	status;		// {no cursor here, ok, arrows ok}
	char	name[10];
	void	(*routine)(int32_t choice);	// choice = menu item #.
	char	alphaKey;	  // hotkey in menu
} menuitem_t;



typedef struct menu_s
{
	int16_t numitems;				// # of menu items
	struct menu_s	*prevMenu;	// previous menu
	menuitem_t		*menuitems;	// menu items
	void	(*routine)(void);	// draw routine
	int16_t	x, y;				// x,y of menu
	int16_t	lastOn;				// last item user was on in menu
} menu_t;

static int16_t itemOn; // menu item skull is on
static int16_t skullAnimCounter; // skull animation counter
static int16_t whichSkull; // which skull to draw

// graphic name of skulls
static char    skullName[2][8] = {"M_SKULL1","M_SKULL2"};

// current menudef
static menu_t *currentMenu;

//
// PROTOTYPES
//
static void M_NewGame(int32_t choice);
static void M_Episode(int32_t choice);
static void M_ChooseSkill(int32_t choice);
static void M_LoadGame(int32_t choice);
static void M_SaveGame(int32_t choice);
static void M_Options(int32_t choice);
static void M_EndGame(int32_t choice);
static void M_ReadThis(int32_t choice);
static void M_ReadThis2(int32_t choice);
static void M_QuitDOOM(int32_t choice);

static void M_ChangeMessages(int32_t choice);
static void M_ChangeSensitivity(int32_t choice);
static void M_SfxVol(int32_t choice);
static void M_MusicVol(int32_t choice);
static void M_ChangeDetail(int32_t choice);
static void M_SizeDisplay(int32_t choice);
static void M_Sound(int32_t choice);

static void M_FinishReadThis(int32_t choice);
static void M_LoadSelect(int32_t choice);
static void M_SaveSelect(int32_t choice);
static void M_ReadSaveStrings(void);
static void M_QuickSave(void);
static void M_QuickLoad(void);

static void M_DrawMainMenu(void);
static void M_DrawReadThis1(void);
static void M_DrawReadThis2(void);
static void M_DrawReadThisRetail(void);
static void M_DrawNewGame(void);
static void M_DrawEpisode(void);
static void M_DrawOptions(void);
static void M_DrawSound(void);
static void M_DrawLoad(void);
static void M_DrawSave(void);

static void M_DrawSaveLoadBorder(int32_t x, int32_t y);
static void M_SetupNextMenu(menu_t *menudef);
static void M_DrawThermo(int32_t x, int32_t y, int32_t thermWidth, int32_t thermDot);
static void M_WriteText(int32_t x, int32_t y, char* string);
static int32_t M_StringWidth(char *string);
static int32_t M_StringHeight(char *string);
static void M_StartMessage(char *string, void routine(int32_t), boolean input);
static void M_ClearMenus(void);




//
// DOOM MENU
//
enum
{
	newgame,
	options,
	loadgame,
	savegame,
	readthis,
	quitdoom,
	main_end
} main_e;

static menuitem_t MainMenu[]=
{
	{1,"M_NGAME", M_NewGame, 'n'},
	{1,"M_OPTION",M_Options, 'o'},
	{1,"M_LOADG", M_LoadGame,'l'},
	{1,"M_SAVEG", M_SaveGame,'s'},
#if (APPVER_DOOMREV < AV_DR_DM19U)
	{1,"M_RDTHIS",M_ReadThis,'r'},
#else
	// Another hickup with Special edition.
	{1,"M_RDTHIS",M_ReadThis2,'r'},
#endif
	{1,"M_QUITG", M_QuitDOOM,'q'}
};

static menu_t  MainDef =
{
	main_end,
	NULL,
	MainMenu,
	M_DrawMainMenu,
	97,64,
	0
};


//
// EPISODE SELECT
//
enum
{
	ep1,
#if !APPVER_CHEX
	ep2,
	ep3,
#if (APPVER_DOOMREV >= AV_DR_DM19UP)
	ep4,
#endif
#endif
	ep_end
} episodes_e;

static menuitem_t EpisodeMenu[]=
{
	{1,"M_EPI1", M_Episode,'k'},
#if !APPVER_CHEX
	{1,"M_EPI2", M_Episode,'t'},
	{1,"M_EPI3", M_Episode,'i'},
#if (APPVER_DOOMREV >= AV_DR_DM19UP)
	{1,"M_EPI4", M_Episode,'t'}
#endif
#endif
};

static menu_t  EpiDef =
{
	ep_end,				// # of menu items
	&MainDef,			// previous menu
	EpisodeMenu,		// menuitem_t ->
	M_DrawEpisode,		// drawing routine ->
	48,63,              // x,y
	ep1					// lastOn
};

//
// NEW GAME
//
enum
{
	killthings,
	toorough,
	hurtme,
	violence,
	nightmare,
	newg_end
} newgame_e;

static menuitem_t NewGameMenu[]=
{
	{1,"M_JKILL",	M_ChooseSkill, 'i'},
	{1,"M_ROUGH",	M_ChooseSkill, 'h'},
	{1,"M_HURT",	M_ChooseSkill, 'h'},
	{1,"M_ULTRA",	M_ChooseSkill, 'u'},
	{1,"M_NMARE",	M_ChooseSkill, 'n'}
};

static menu_t  NewDef =
{
	newg_end,			// # of menu items
	&EpiDef,			// previous menu
	NewGameMenu,		// menuitem_t ->
	M_DrawNewGame,		// drawing routine ->
	48,63,              // x,y
	hurtme				// lastOn
};



//
// OPTIONS MENU
//
enum
{
	endgame,
	messages,
	detail,
	scrnsize,
	option_empty1,
	mousesens,
	option_empty2,
	soundvol,
	opt_end
} options_e;

static menuitem_t OptionsMenu[]=
{
	{1,"M_ENDGAM",	M_EndGame,'e'},
	{1,"M_MESSG",	M_ChangeMessages,'m'},
	{1,"M_DETAIL",	M_ChangeDetail,'g'},
	{2,"M_SCRNSZ",	M_SizeDisplay,'s'},
	{-1,"",0},
	{2,"M_MSENS",	M_ChangeSensitivity,'m'},
	{-1,"",0},
	{1,"M_SVOL",	M_Sound,'s'}
};

static menu_t  OptionsDef =
{
	opt_end,
	&MainDef,
	OptionsMenu,
	M_DrawOptions,
	60,37,
	0
};

//
// Read This! MENU 1 & 2
//
enum
{
	rdthsempty1,
	read1_end
} read_e;

static menuitem_t ReadMenu1[] =
{
	{1,"",M_ReadThis2,0}
};

static menu_t  ReadDef1 =
{
	read1_end,
	&MainDef,
	ReadMenu1,
	M_DrawReadThis1,
	280,185,
	0
};

enum
{
	rdthsempty2,
	read2_end
} read_e2;

static menuitem_t ReadMenu2[]=
{
	{1,"",M_FinishReadThis,0}
};

static menu_t  ReadDef2 =
{
	read2_end,
#if (APPVER_DOOMREV < AV_DR_DM19U)
	&ReadDef1,
#else
	NULL,
#endif
	ReadMenu2,
	M_DrawReadThis2,
	330,175,
	0
};

//
// SOUND VOLUME MENU
//
enum
{
	sfx_vol,
	sfx_empty1,
	music_vol,
	sfx_empty2,
	sound_end
} sound_e;

static menuitem_t SoundMenu[]=
{
	{2,"M_SFXVOL",M_SfxVol,'s'},
	{-1,"",0},
	{2,"M_MUSVOL",M_MusicVol,'m'},
	{-1,"",0}
};

static menu_t  SoundDef =
{
	sound_end,
	&OptionsDef,
	SoundMenu,
	M_DrawSound,
	80,64,
	0
};

//
// LOAD GAME MENU
//
enum
{
	load1,
	load2,
	load3,
	load4,
	load5,
	load6,
	load_end
} load_e;

static menuitem_t LoadMenu[]=
{
	{1,"", M_LoadSelect,'1'},
	{1,"", M_LoadSelect,'2'},
	{1,"", M_LoadSelect,'3'},
	{1,"", M_LoadSelect,'4'},
	{1,"", M_LoadSelect,'5'},
	{1,"", M_LoadSelect,'6'}
};

static menu_t  LoadDef =
{
	load_end,
	&MainDef,
	LoadMenu,
	M_DrawLoad,
	80,54,
	0
};

//
// SAVE GAME MENU
//
static menuitem_t SaveMenu[]=
{
	{1,"", M_SaveSelect,'1'},
	{1,"", M_SaveSelect,'2'},
	{1,"", M_SaveSelect,'3'},
	{1,"", M_SaveSelect,'4'},
	{1,"", M_SaveSelect,'5'},
	{1,"", M_SaveSelect,'6'}
};

static menu_t  SaveDef =
{
	load_end,
	&MainDef,
	SaveMenu,
	M_DrawSave,
	80,54,
	0
};


//
// M_ReadSaveStrings
//  read the strings from the savegame files
//
static void M_ReadSaveStrings(void)
{
	int32_t             handle;
	int32_t             i;
	char    name[256];

	for (i = 0;i < load_end;i++)
	{
#if (APPVER_DOOMREV >= AV_DR_DM1666E)
		if (M_CheckParm("-cdrom"))
			sprintf(name, "c:\\doomdata\\"SAVEGAMENAME"%d.dsg", i);
		else
#endif
			sprintf(name, SAVEGAMENAME"%d.dsg", i);

		handle = open(name, O_RDONLY | 0, 0666);
		if (handle == -1)
		{
			strcpy(&savegamestrings[i][0], EMPTYSTRING);
			LoadMenu[i].status = 0;
			continue;
		}
		read(handle, &savegamestrings[i], SAVESTRINGSIZE);
		close(handle);
		LoadMenu[i].status = 1;
	}
}


//
// M_LoadGame & Cie.
//
static void M_DrawLoad(void)
{
	int32_t             i;

	V_DrawPatchDirect (72,28,W_CacheLumpName("M_LOADG",PU_CACHE));
	for (i = 0;i < load_end; i++)
	{
		M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
		M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i]);
	}
}



//
// Draw border for the savegame description
//
static void M_DrawSaveLoadBorder(int32_t x,int32_t y)
{
	int32_t             i;
	
	V_DrawPatchDirect (x-8,y+7,W_CacheLumpName("M_LSLEFT",PU_CACHE));

	for (i = 0;i < 24;i++)
	{
		V_DrawPatchDirect (x,y+7,W_CacheLumpName("M_LSCNTR",PU_CACHE));
		x += 8;
	}

	V_DrawPatchDirect (x,y+7,W_CacheLumpName("M_LSRGHT",PU_CACHE));
}



//
// User wants to load this game
//
static void M_LoadSelect(int32_t choice)
{
	char    name[256];
	
#if (APPVER_DOOMREV >= AV_DR_DM1666E)
	if (M_CheckParm("-cdrom"))
		sprintf(name,"c:\\doomdata\\"SAVEGAMENAME"%d.dsg",choice);
	else
#endif
		sprintf(name,SAVEGAMENAME"%d.dsg",choice);
	G_LoadGame (name);
	M_ClearMenus ();
}

//
// Selected from DOOM menu
//
static void M_LoadGame (int32_t choice)
{
	UNUSED(choice);

	if (netgame)
	{
		M_StartMessage(LOADNET,NULL,false);
		return;
	}
	
	M_SetupNextMenu(&LoadDef);
	M_ReadSaveStrings();
}


//
//  M_SaveGame & Cie.
//
static void M_DrawSave(void)
{
	int32_t             i;
	
	V_DrawPatchDirect (72,28,W_CacheLumpName("M_SAVEG",PU_CACHE));
	for (i = 0;i < load_end; i++)
	{
		M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
		M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i]);
	}
	
	if (saveStringEnter)
	{
		i = M_StringWidth(savegamestrings[saveSlot]);
		M_WriteText(LoadDef.x + i,LoadDef.y+LINEHEIGHT*saveSlot,"_");
	}
}

//
// M_Responder calls this when user is finished
//
static void M_DoSave(int32_t slot)
{
	G_SaveGame (slot,savegamestrings[slot]);
	M_ClearMenus ();

	// PICK QUICKSAVE SLOT YET?
	if (quickSaveSlot == -2)
		quickSaveSlot = slot;
}

//
// User wants to save. Start string input for M_Responder
//
static void M_SaveSelect(int32_t choice)
{
	// we are going to be intercepting all chars
	saveStringEnter = 1;
    
	saveSlot = choice;
	strcpy(saveOldString,savegamestrings[choice]);
	if (!strcmp(savegamestrings[choice],EMPTYSTRING))
		savegamestrings[choice][0] = 0;
	saveCharIndex = strlen(savegamestrings[choice]);
}

//
// Selected from DOOM menu
//
static void M_SaveGame (int32_t choice)
{
	UNUSED(choice);

	if (!usergame)
	{
		M_StartMessage(SAVEDEAD,NULL,false);
		return;
	}
	
	if (gamestate != GS_LEVEL)
		return;
	
	M_SetupNextMenu(&SaveDef);
	M_ReadSaveStrings();
}



//
//      M_QuickSave
//
static char    tempstring[80];

static void M_QuickSaveResponse(int32_t ch)
{
	if (ch == 'y')
	{
		M_DoSave(quickSaveSlot);
		S_StartSound(NULL,sfx_swtchx);
	}
}

static void M_QuickSave(void)
{
	if (!usergame)
	{
		S_StartSound(NULL,sfx_oof);
		return;
	}

	if (gamestate != GS_LEVEL)
		return;
	
	if (quickSaveSlot < 0)
	{
		M_StartControlPanel();
		M_ReadSaveStrings();
		M_SetupNextMenu(&SaveDef);
		quickSaveSlot = -2;	// means to pick a slot now
		return;
	}
	sprintf(tempstring,QSPROMPT,savegamestrings[quickSaveSlot]);
	M_StartMessage(tempstring,M_QuickSaveResponse,true);
}



//
// M_QuickLoad
//
static void M_QuickLoadResponse(int32_t ch)
{
	if (ch == 'y')
	{
		M_LoadSelect(quickSaveSlot);
		S_StartSound(NULL,sfx_swtchx);
	}
}


static void M_QuickLoad(void)
{
	if (netgame)
	{
		M_StartMessage(QLOADNET,NULL,false);
		return;
	}
	
	if (quickSaveSlot < 0)
	{
		M_StartMessage(QSAVESPOT,NULL,false);
		return;
	}
	sprintf(tempstring,QLPROMPT,savegamestrings[quickSaveSlot]);
	M_StartMessage(tempstring,M_QuickLoadResponse,true);
}




//
// Read This Menus
//
static void M_DrawReadThis1(void)
{
	inhelpscreens = true;
	V_DrawPatchDirect (0,0,W_CacheLumpName("HELP2",PU_CACHE));
}

//
// Read This Menus - optional second page.
//
static void M_DrawReadThis2(void)
{
	inhelpscreens = true;
#if (APPVER_DOOMREV < AV_DR_DM19F) || APPVER_CHEX
	V_DrawPatchDirect (0,0,W_CacheLumpName("HELP1",PU_CACHE));
#else
	V_DrawPatchDirect (0,0,W_CacheLumpName("HELP",PU_CACHE));
#endif
}

static void M_DrawReadThisRetail(void)
{
	inhelpscreens = true;
	V_DrawPatchDirect (0,0,W_CacheLumpName("HELP",PU_CACHE));
}


//
// Change Sfx & Music volumes
//
static void M_DrawSound(void)
{
	V_DrawPatchDirect (60,38,W_CacheLumpName("M_SVOL",PU_CACHE));

	M_DrawThermo(SoundDef.x,SoundDef.y+LINEHEIGHT*(sfx_vol+1),
				 16,sfxVolume);

	M_DrawThermo(SoundDef.x,SoundDef.y+LINEHEIGHT*(music_vol+1),
				 16,musicVolume);
}

static void M_Sound(int32_t choice)
{
	UNUSED(choice);
	M_SetupNextMenu(&SoundDef);
}

static void M_SfxVol(int32_t choice)
{
	switch (choice)
	{
		case 0:
			if (sfxVolume)
				sfxVolume--;
			break;
		case 1:
			if (sfxVolume < 15)
				sfxVolume++;
			break;
	}
	
	S_SetSfxVolume(sfxVolume*8);
}

static void M_MusicVol(int32_t choice)
{
	switch (choice)
	{
		case 0:
			if (musicVolume)
				musicVolume--;
			break;
		case 1:
			if (musicVolume < 15)
				musicVolume++;
			break;
	}
	
    S_SetMusicVolume(musicVolume*8);
}




//
// M_DrawMainMenu
//
static void M_DrawMainMenu(void)
{
	V_DrawPatchDirect (94,2,W_CacheLumpName("M_DOOM",PU_CACHE));
}




//
// M_NewGame
//
static void M_DrawNewGame(void)
{
	V_DrawPatchDirect (96,14,W_CacheLumpName("M_NEWG",PU_CACHE));
	V_DrawPatchDirect (54,38,W_CacheLumpName("M_SKILL",PU_CACHE));
}

static void M_NewGame(int32_t choice)
{
	UNUSED(choice);

	if (netgame && !demoplayback)
	{
		M_StartMessage(NEWGAME,NULL,false);
		return;
	}
	
#if APPVER_CHEX
	M_SetupNextMenu(&NewDef);
#else
	if (commercial)
		M_SetupNextMenu(&NewDef);
	else
		M_SetupNextMenu(&EpiDef);
#endif
}


//
//      M_Episode
//
static int32_t     epi;

static void M_DrawEpisode(void)
{
	V_DrawPatchDirect (54,38,W_CacheLumpName("M_EPISOD",PU_CACHE));
}

static void M_VerifyNightmare(int32_t ch)
{
	if (ch != 'y')
		return;
		
	G_DeferedInitNew(nightmare,epi+1,1);
	M_ClearMenus ();
}

static void M_ChooseSkill(int32_t choice)
{
	if (choice == nightmare)
	{
		M_StartMessage(NIGHTMARE,M_VerifyNightmare,true);
		return;
	}
	
	G_DeferedInitNew(choice,epi+1,1);
	M_ClearMenus ();
}

static void M_Episode(int32_t choice)
{
	if (shareware && choice)
	{
		M_StartMessage(SWSTRING,NULL,false);
		M_SetupNextMenu(&ReadDef1);
		return;
	}

	epi = choice;
	M_SetupNextMenu(&NewDef);
}



//
// M_Options
//
static char    detailNames[2][9]	= {"M_GDHIGH","M_GDLOW"};
static char	   msgNames[2][9]		= {"M_MSGOFF","M_MSGON"};


static void M_DrawOptions(void)
{
	V_DrawPatchDirect (108,15,W_CacheLumpName("M_OPTTTL",PU_CACHE));
	
	V_DrawPatchDirect (OptionsDef.x + 175,OptionsDef.y+LINEHEIGHT*detail,W_CacheLumpName(detailNames[detailLevel],PU_CACHE));

	V_DrawPatchDirect (OptionsDef.x + 120,OptionsDef.y+LINEHEIGHT*messages,W_CacheLumpName(msgNames[showMessages],PU_CACHE));

	M_DrawThermo(OptionsDef.x,OptionsDef.y+LINEHEIGHT*(mousesens+1),
				 10,mouseSensitivity);
	
	M_DrawThermo(OptionsDef.x,OptionsDef.y+LINEHEIGHT*(scrnsize+1),
				 9,screenSize);
}

static void M_Options(int32_t choice)
{
	UNUSED(choice);
	M_SetupNextMenu(&OptionsDef);
}



//
//      Toggle messages on/off
//
static void M_ChangeMessages(int32_t choice)
{
	UNUSED(choice);

	showMessages = 1 - showMessages;

	if (showMessages)
		players[consoleplayer].message = MSGON ;
	else
		players[consoleplayer].message = MSGOFF;

	message_dontfuckwithme = true;
}


//
// M_EndGame
//
static void M_EndGameResponse(int32_t ch)
{
	if (ch != 'y')
		return;
		
	currentMenu->lastOn = itemOn;
	M_ClearMenus ();
	D_StartTitle ();
}

static void M_EndGame(int32_t choice)
{
	UNUSED(choice);

	if (!usergame)
	{
		S_StartSound(NULL,sfx_oof);
		return;
	}

	if (netgame)
	{
		M_StartMessage(NETEND,NULL,false);
		return;
	}

	M_StartMessage(ENDGAME,M_EndGameResponse,true);
}




//
// M_ReadThis
//
static void M_ReadThis(int32_t choice)
{
	UNUSED(choice);
	M_SetupNextMenu(&ReadDef1);
}

static void M_ReadThis2(int32_t choice)
{
	UNUSED(choice);
	M_SetupNextMenu(&ReadDef2);
}

static void M_FinishReadThis(int32_t choice)
{
	UNUSED(choice);
	M_SetupNextMenu(&MainDef);
}




//
// M_QuitDOOM
//
static int32_t     quitsounds[8] =
{
	sfx_pldeth,
	sfx_dmpain,
	sfx_popain,
	sfx_slop,
	sfx_telept,
	sfx_posit1,
	sfx_posit3,
	sfx_sgtatk
};

static int32_t     quitsounds2[8] =
{
	sfx_vilact,
	sfx_getpow,
	sfx_boscub,
	sfx_slop,
	sfx_skeswg,
	sfx_kntdth,
	sfx_bspact,
	sfx_sgtatk
};



static void M_QuitResponse(int32_t ch)
{
	if (ch != 'y')
		return;
	if (!netgame)
	{
		if (commercial)
			S_StartSound(NULL,quitsounds2[(gametic>>2)&7]);
		else
			S_StartSound(NULL,quitsounds[(gametic>>2)&7]);
		I_WaitVBL(105);
	}
	I_Quit ();
}




static void M_QuitDOOM(int32_t choice)
{
	UNUSED(choice);

	if (commercial)
	{
#if (APPVER_DOOMREV >= AV_DR_DM18FR)
		if (french)
			sprintf(endstring,"%s\n\n"DOSY,endmsg2[0]);
		else
#endif
			sprintf(endstring,"%s\n\n"DOSY,endmsg2[(gametic>>2)&7]);
	}
	else
		sprintf(endstring,"%s\n\n"DOSY,endmsg[(gametic>>2)&7]);

	M_StartMessage(endstring,M_QuitResponse,true);
}




static void M_ChangeSensitivity(int32_t choice)
{
	switch (choice)
	{
		case 0:
			if (mouseSensitivity)
				mouseSensitivity--;
			break;
		case 1:
			if (mouseSensitivity < 9)
				mouseSensitivity++;
			break;
	}
}




static void M_ChangeDetail(int32_t choice)
{
	UNUSED(choice);

	detailLevel = 1 - detailLevel;
    
	R_SetViewSize (screenblocks, detailLevel);
	if (!detailLevel)
		players[consoleplayer].message = DETAILHI;
	else
		players[consoleplayer].message = DETAILLO;
}




static void M_SizeDisplay(int32_t choice)
{
	switch (choice)
	{
	case 0:
		if (screenSize > 0)
		{
			screenblocks--;
			screenSize--;
		}
		break;
	case 1:
		if (screenSize < 8)
		{
			screenblocks++;
			screenSize++;
		}
		break;
	}
	

	R_SetViewSize (screenblocks, detailLevel);
}




//
//      Menu Functions
//
static void M_DrawThermo(int32_t x, int32_t y, int32_t thermWidth, int32_t thermDot)
{
	int32_t		xx;
	int32_t		i;

	xx = x;
	V_DrawPatchDirect (xx,y,W_CacheLumpName("M_THERML",PU_CACHE));
	xx += 8;
	for (i=0;i<thermWidth;i++)
	{
		V_DrawPatchDirect (xx,y,W_CacheLumpName("M_THERMM",PU_CACHE));
		xx += 8;
	}
	V_DrawPatchDirect (xx,y,W_CacheLumpName("M_THERMR",PU_CACHE));

	V_DrawPatchDirect ((x+8) + thermDot*8,y,W_CacheLumpName("M_THERMO",PU_CACHE));
}



static void M_StartMessage(char *string, void routine(int32_t), boolean input)
{
	messageLastMenuActive = menuactive;
	messageToPrint = 1;
	messageString = string;
	messageRoutine = routine;
	messageNeedsInput = input;
	menuactive = true;
	return;
}



//
// Find string width from hu_font chars
//
static int32_t M_StringWidth(char *string)
{
	int32_t             i;
	int32_t             w = 0;
	int32_t             c;
	
	for (i = 0;i < strlen(string);i++)
	{
		c = toupper(string[i]) - HU_FONTSTART;
		if (c < 0 || c >= HU_FONTSIZE)
			w += 4;
		else
			w += SHORT (hu_font[c]->width);
	}
		
	return w;
}



//
//      Find string height from hu_font chars
//
static int32_t M_StringHeight(char *string)
{
	int32_t             i;
	int32_t             h;
	int32_t             height = SHORT(hu_font[0]->height);
	
	h = height;
	for (i = 0;i < strlen(string);i++)
		if (string[i] == '\n')
			h += height;
		
	return h;
}


//
//      Write a string using the hu_font
//
static void M_WriteText(int32_t x, int32_t y, char *string)
{
	int32_t		w;
	char*	ch;
	int32_t		c;
	int32_t		cx;
	int32_t		cy;
		

	ch = string;
	cx = x;
	cy = y;
	
	while(1)
	{
		c = *ch++;
		if (!c)
			break;
		if (c == '\n')
		{
			cx = x;
			cy += 12;
			continue;
		}
		
		c = toupper(c) - HU_FONTSTART;
		if (c < 0 || c>= HU_FONTSIZE)
		{
			cx += 4;
			continue;
		}
		
		w = SHORT (hu_font[c]->width);
		if (cx+w > SCREENWIDTH)
			break;
		V_DrawPatchDirect(cx, cy, hu_font[c]);
		cx+=w;
	}
}



//
// CONTROL PANEL
//

//
// M_Responder
//
boolean M_Responder (event_t* ev)
{
	int32_t             ch;
	int32_t             i;
	static  int32_t     joywait = 0;
	static  int32_t     mousewait = 0;
	static  int32_t     mousey = 0;
	static  int32_t     lasty = 0;
	static  int32_t     mousex = 0;
	static  int32_t     lastx = 0;
	
	ch = -1;
	
	if (ev->type == ev_joystick && joywait < I_GetTime())
	{
		if (ev->data3 == -1)
		{
			ch = KEY_UPARROW;
			joywait = I_GetTime() + 5;
		}
		else if (ev->data3 == 1)
		{
			ch = KEY_DOWNARROW;
			joywait = I_GetTime() + 5;
		}
		
		if (ev->data2 == -1)
		{
			ch = KEY_LEFTARROW;
			joywait = I_GetTime() + 2;
		}
		else if (ev->data2 == 1)
		{
			ch = KEY_RIGHTARROW;
			joywait = I_GetTime() + 2;
		}
		
		if (ev->data1&1)
		{
			ch = KEY_ENTER;
			joywait = I_GetTime() + 5;
		}
		if (ev->data1&2)
		{
			ch = KEY_BACKSPACE;
			joywait = I_GetTime() + 5;
		}
	}
	else
	{
		if (ev->type == ev_mouse && mousewait < I_GetTime())
		{
			mousey += ev->data3;
			if (mousey < lasty-30)
			{
				ch = KEY_DOWNARROW;
				mousewait = I_GetTime() + 5;
				mousey = lasty -= 30;
			}
			else if (mousey > lasty+30)
			{
				ch = KEY_UPARROW;
				mousewait = I_GetTime() + 5;
				mousey = lasty += 30;
			}
		
			mousex += ev->data2;
			if (mousex < lastx-30)
			{
				ch = KEY_LEFTARROW;
				mousewait = I_GetTime() + 5;
				mousex = lastx -= 30;
			}
			else if (mousex > lastx+30)
			{
				ch = KEY_RIGHTARROW;
				mousewait = I_GetTime() + 5;
				mousex = lastx += 30;
			}
		
			if (ev->data1&1)
			{
				ch = KEY_ENTER;
				mousewait = I_GetTime() + 15;
			}
			
			if (ev->data1&2)
			{
			ch = KEY_BACKSPACE;
			mousewait = I_GetTime() + 15;
			}
		}
		else
			if (ev->type == ev_keydown)
			{
				ch = ev->data1;
			}
	}
    
	if (ch == -1)
		return false;

    
	// Save Game string input
	if (saveStringEnter)
	{
		switch(ch)
		{
			case KEY_BACKSPACE:
				if (saveCharIndex > 0)
				{
					saveCharIndex--;
					savegamestrings[saveSlot][saveCharIndex] = 0;
				}
				break;
				
			case KEY_ESCAPE:
				saveStringEnter = 0;
				strcpy(&savegamestrings[saveSlot][0],saveOldString);
				break;
				
			case KEY_ENTER:
				saveStringEnter = 0;
				if (savegamestrings[saveSlot][0])
				M_DoSave(saveSlot);
				break;
				
			default:
				ch = toupper(ch);
				if (ch != 32)
					if (ch-HU_FONTSTART < 0 || ch-HU_FONTSTART >= HU_FONTSIZE)
						break;
				if (ch >= 32 && ch <= 127 &&
					saveCharIndex < SAVESTRINGSIZE-1 &&
					M_StringWidth(savegamestrings[saveSlot]) <
					(SAVESTRINGSIZE-2)*8)
				{
					savegamestrings[saveSlot][saveCharIndex++] = ch;
					savegamestrings[saveSlot][saveCharIndex] = 0;
				}
				break;
		}
		return true;
	}
    
	// Take care of any messages that need input
	if (messageToPrint)
	{
		if (messageNeedsInput == true &&
			!(ch == ' ' || ch == 'n' || ch == 'y' || ch == KEY_ESCAPE))
			return false;
		
		menuactive = messageLastMenuActive;
		messageToPrint = 0;
		if (messageRoutine)
			messageRoutine(ch);
			
		menuactive = false;
		S_StartSound(NULL,sfx_swtchx);
		return true;
	}
	
	if (devparm && ch == KEY_F1)
	{
		G_ScreenShot ();
		return true;
	}
		
    
	// F-Keys
	if (!menuactive)
		switch(ch)
		{
			case KEY_MINUS:         // Screen size down
				if (automapactive || chat_on)
					return false;
				M_SizeDisplay(0);
				S_StartSound(NULL,sfx_stnmov);
				return true;
				
			case KEY_EQUALS:        // Screen size up
				if (automapactive || chat_on)
					return false;
				M_SizeDisplay(1);
				S_StartSound(NULL,sfx_stnmov);
				return true;
				
			case KEY_F1:            // Help key
				M_StartControlPanel ();

#if (APPVER_DOOMREV < AV_DR_DM19U)
				currentMenu = &ReadDef1;
#else
				currentMenu = &ReadDef2;
#endif
	    
				itemOn = 0;
				S_StartSound(NULL,sfx_swtchn);
				return true;
				
			case KEY_F2:            // Save
				M_StartControlPanel();
				S_StartSound(NULL,sfx_swtchn);
				M_SaveGame(0);
				return true;
				
			case KEY_F3:            // Load
				M_StartControlPanel();
				S_StartSound(NULL,sfx_swtchn);
				M_LoadGame(0);
				return true;
				
			case KEY_F4:            // Sound Volume
				M_StartControlPanel ();
				currentMenu = &SoundDef;
				itemOn = sfx_vol;
				S_StartSound(NULL,sfx_swtchn);
				return true;
				
			case KEY_F5:            // Detail toggle
				M_ChangeDetail(0);
				S_StartSound(NULL,sfx_swtchn);
				return true;
				
			case KEY_F6:            // Quicksave
				S_StartSound(NULL,sfx_swtchn);
				M_QuickSave();
				return true;
				
			case KEY_F7:            // End game
				S_StartSound(NULL,sfx_swtchn);
				M_EndGame(0);
				return true;
				
			case KEY_F8:            // Toggle messages
				M_ChangeMessages(0);
				S_StartSound(NULL,sfx_swtchn);
				return true;
				
			case KEY_F9:            // Quickload
				S_StartSound(NULL,sfx_swtchn);
				M_QuickLoad();
				return true;
				
			case KEY_F10:           // Quit DOOM
				S_StartSound(NULL,sfx_swtchn);
				M_QuitDOOM(0);
				return true;
				
			case KEY_F11:           // gamma toggle
				usegamma++;
				if (usegamma > 4)
					usegamma = 0;
				players[consoleplayer].message = gammamsg[usegamma];
				I_SetPalette (W_CacheLumpName ("PLAYPAL",PU_CACHE));
				return true;
				
		}

    
	// Pop-up menu?
	if (!menuactive)
	{
		if (ch == KEY_ESCAPE)
		{
			M_StartControlPanel ();
			S_StartSound(NULL,sfx_swtchn);
			return true;
		}
		return false;
	}

    
	// Keys usable within menu
	switch (ch)
	{
		case KEY_DOWNARROW:
			do
			{
				if (itemOn+1 > currentMenu->numitems-1)
					itemOn = 0;
				else itemOn++;
				S_StartSound(NULL,sfx_pstop);
			} while(currentMenu->menuitems[itemOn].status==-1);
			return true;
		
		case KEY_UPARROW:
			do
			{
				if (!itemOn)
					itemOn = currentMenu->numitems-1;
				else itemOn--;
				S_StartSound(NULL,sfx_pstop);
			} while(currentMenu->menuitems[itemOn].status==-1);
			return true;

		case KEY_LEFTARROW:
			if (currentMenu->menuitems[itemOn].routine &&
				currentMenu->menuitems[itemOn].status == 2)
			{
				S_StartSound(NULL,sfx_stnmov);
				currentMenu->menuitems[itemOn].routine(0);
			}
			return true;
		
		case KEY_RIGHTARROW:
			if (currentMenu->menuitems[itemOn].routine &&
				currentMenu->menuitems[itemOn].status == 2)
			{
				S_StartSound(NULL,sfx_stnmov);
				currentMenu->menuitems[itemOn].routine(1);
			}
			return true;

		case KEY_ENTER:
			if (currentMenu->menuitems[itemOn].routine &&
				currentMenu->menuitems[itemOn].status)
			{
				currentMenu->lastOn = itemOn;
				if (currentMenu->menuitems[itemOn].status == 2)
				{
					currentMenu->menuitems[itemOn].routine(1);      // right arrow
					S_StartSound(NULL,sfx_stnmov);
				}
				else
				{
					currentMenu->menuitems[itemOn].routine(itemOn);
					S_StartSound(NULL,sfx_pistol);
				}
			}
			return true;
		
		case KEY_ESCAPE:
			currentMenu->lastOn = itemOn;
			M_ClearMenus ();
			S_StartSound(NULL,sfx_swtchx);
			return true;
		
		case KEY_BACKSPACE:
			currentMenu->lastOn = itemOn;
			if (currentMenu->prevMenu)
			{
				currentMenu = currentMenu->prevMenu;
				itemOn = currentMenu->lastOn;
				S_StartSound(NULL,sfx_swtchn);
			}
			return true;
	
		default:
			for (i = itemOn+1;i < currentMenu->numitems;i++)
			if (currentMenu->menuitems[i].alphaKey == ch)
			{
				itemOn = i;
				S_StartSound(NULL,sfx_pstop);
				return true;
			}
			for (i = 0;i <= itemOn;i++)
				if (currentMenu->menuitems[i].alphaKey == ch)
				{
					itemOn = i;
					S_StartSound(NULL,sfx_pstop);
					return true;
				}
			break;
	
	}

	return false;
}



//
// M_StartControlPanel
//
void M_StartControlPanel (void)
{
	// intro might call this repeatedly
	if (menuactive)
		return;

	menuactive = 1;
	currentMenu = &MainDef;         // JDC
	itemOn = currentMenu->lastOn;   // JDC
}


//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer (void)
{
	static int16_t	x;
	static int16_t	y;
	int16_t		i;
	int16_t		max;
	char		string[40];
	int32_t			start;

	inhelpscreens = false;

    
	// Horiz. & Vertically center string and print it.
	if (messageToPrint)
	{
		start = 0;
		y = 100 - M_StringHeight(messageString)/2;
		while(*(messageString+start))
		{
			for (i = 0;i < strlen(messageString+start);i++)
			if (*(messageString+start+i) == '\n')
			{
				memset(string,0,40);
				strncpy(string,messageString+start,i);
				start += i+1;
				break;
			}
				
			if (i == strlen(messageString+start))
			{
				strcpy(string,messageString+start);
				start += i;
			}
				
			x = 160 - M_StringWidth(string)/2;
			M_WriteText(x,y,string);
			y += SHORT(hu_font[0]->height);
		}
		return;
	}

	if (!menuactive)
		return;

	if (currentMenu->routine)
		currentMenu->routine();         // call Draw routine
    
	// DRAW MENU
	x = currentMenu->x;
	y = currentMenu->y;
	max = currentMenu->numitems;

	for (i=0;i<max;i++)
	{
		if (currentMenu->menuitems[i].name[0])
			V_DrawPatchDirect (x,y,W_CacheLumpName(currentMenu->menuitems[i].name ,PU_CACHE));
		y += LINEHEIGHT;
	}

    
	// DRAW SKULL
	V_DrawPatchDirect(x + SKULLXOFF,currentMenu->y - 5 + itemOn*LINEHEIGHT, W_CacheLumpName(skullName[whichSkull],PU_CACHE));

}


//
// M_ClearMenus
//
static void M_ClearMenus (void)
{
	menuactive = 0;
	// if (!netgame && usergame && paused)
	//       sendpause = true;
}




//
// M_SetupNextMenu
//
static void M_SetupNextMenu(menu_t *menudef)
{
	currentMenu = menudef;
	itemOn = currentMenu->lastOn;
}


//
// M_Ticker
//
void M_Ticker (void)
{
	if (--skullAnimCounter <= 0)
	{
		whichSkull ^= 1;
		skullAnimCounter = 8;
	}
}


//
// M_Init
//
void M_Init (void)
{
	currentMenu = &MainDef;
	menuactive = 0;
	itemOn = currentMenu->lastOn;
	whichSkull = 0;
	skullAnimCounter = 10;
	screenSize = screenblocks - 3;
	messageToPrint = 0;
	messageString = NULL;
	messageLastMenuActive = menuactive;
	quickSaveSlot = -1;

	if (commercial)
	{
		MainMenu[readthis] = MainMenu[quitdoom];
		MainDef.numitems--;
		MainDef.y += 8;
		NewDef.prevMenu = &MainDef;
		ReadDef1.routine = M_DrawReadThisRetail;
		ReadDef1.x = 330;
		ReadDef1.y = 165;
		ReadMenu1[0].routine = M_FinishReadThis;
	}
}
