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

// I_IBM.C

#include <conio.h>
#include <dos.h>
#include <stdarg.h>
#include "doomdef.h"
#include "r_local.h"

#define DPMI_INT 0x31
//#define NOTIMER

void I_StartupSound (void);
void I_ShutdownSound (void);

void I_ShutdownTimer (void);

static void DPMIInt (int32_t i);

static void I_ReadMouse (void);
static void I_InitDiskFlash (void);

extern  int32_t     usemouse, usejoystick;


/*
=============================================================================

							CONSTANTS

=============================================================================
*/

#define SC_INDEX                0x3c4
#define SC_RESET                0
#define SC_CLOCK                1
#define SC_MAPMASK              2
#define SC_CHARMAP              3
#define SC_MEMMODE              4

#define CRTC_INDEX              0x3d4
#define CRTC_H_TOTAL    0
#define CRTC_H_DISPEND  1
#define CRTC_H_BLANK    2
#define CRTC_H_ENDBLANK 3
#define CRTC_H_RETRACE  4
#define CRTC_H_ENDRETRACE 5
#define CRTC_V_TOTAL    6
#define CRTC_OVERFLOW   7
#define CRTC_ROWSCAN    8
#define CRTC_MAXSCANLINE 9
#define CRTC_CURSORSTART 10
#define CRTC_CURSOREND  11
#define CRTC_STARTHIGH  12
#define CRTC_STARTLOW   13
#define CRTC_CURSORHIGH 14
#define CRTC_CURSORLOW  15
#define CRTC_V_RETRACE  16
#define CRTC_V_ENDRETRACE 17
#define CRTC_V_DISPEND  18
#define CRTC_OFFSET             19
#define CRTC_UNDERLINE  20
#define CRTC_V_BLANK    21
#define CRTC_V_ENDBLANK 22
#define CRTC_MODE               23
#define CRTC_LINECOMPARE 24


#define GC_INDEX                0x3ce
#define GC_SETRESET             0
#define GC_ENABLESETRESET 1
#define GC_COLORCOMPARE 2
#define GC_DATAROTATE   3
#define GC_READMAP              4
#define GC_MODE                 5
#define GC_MISCELLANEOUS 6
#define GC_COLORDONTCARE 7
#define GC_BITMASK              8

#define ATR_INDEX               0x3c0
#define ATR_MODE                16
#define ATR_OVERSCAN    17
#define ATR_COLORPLANEENABLE 18
#define ATR_PELPAN              19
#define ATR_COLORSELECT 20

#define STATUS_REGISTER_1    0x3da

#define PEL_WRITE_ADR   0x3c8
#define PEL_READ_ADR    0x3c7
#define PEL_DATA                0x3c9
#define PEL_MASK                0x3c6

static boolean grmode;

//==================================================
//
// joystick vars
//
//==================================================

static boolean         joystickpresent;
static uint32_t        joystickx, joysticky;
static boolean I_ReadJoystick (void)	// returns false if not connected
{
	// TODO implement joystick support
	return false;
}


//==================================================

#define KEYBOARDINT 9

#define CRTCOFF (_inbyte(STATUS_REGISTER_1)&1)
#define CLI     _disable()
#define STI     _enable()

#define _outbyte(x,y) (outp(x,y))
#define _outhword(x,y) (outpw(x,y))

#define _inbyte(x) (inp(x))
#define _inhword(x) (inpw(x))

#define MOUSEB1 1
#define MOUSEB2 2
#define MOUSEB3 4

static  boolean mousepresent;

//===============================

int32_t             ticcount;

// REGS stuff used for int calls
static union REGS regs;

static boolean novideo; // if true, stay in text mode for debugging

#define KBDQUESIZE 32
static byte keyboardque[KBDQUESIZE];
static int32_t kbdtail, kbdhead;

#define KEY_LSHIFT      0xfe

#define KEY_INS         (0x80+0x52)
#define KEY_DEL         (0x80+0x53)
#define KEY_PGUP        (0x80+0x49)
#define KEY_PGDN        (0x80+0x51)
#define KEY_HOME        (0x80+0x47)
#define KEY_END         (0x80+0x4f)

#define SC_RSHIFT       0x36
#define SC_LSHIFT       0x2a

byte        scantokey[128] =
					{
//  0           1       2       3       4       5       6       7
//  8           9       A       B       C       D       E       F
	0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6',
	'7',    '8',    '9',    '0',    '-',    '=',    KEY_BACKSPACE, 9, // 0
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
	'o',    'p',    '[',    ']',    13 ,    KEY_RCTRL,'a',  's',      // 1
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
	39 ,    '`',    KEY_LSHIFT,92,  'z',    'x',    'c',    'v',      // 2
	'b',    'n',    'm',    ',',    '.',    '/',    KEY_RSHIFT,'*',
	KEY_RALT,' ',   0  ,    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,   // 3
	KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,0  ,    0  , KEY_HOME,
	KEY_UPARROW,KEY_PGUP,'-',KEY_LEFTARROW,'5',KEY_RIGHTARROW,'+',KEY_END, //4
	KEY_DOWNARROW,KEY_PGDN,KEY_INS,KEY_DEL,0,0,             0,              KEY_F11,
	KEY_F12,0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 5
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7
					};

//==========================================================================

/*
===================
=
= I_BaseTiccmd
=
===================
*/

typedef struct
{
	int32_t f_0; // interrupt
	ticcmd_t f_4; // cmd
} doomcontrol_t;

static doomcontrol_t *doomcon;
static ticcmd_t emptycmd;

ticcmd_t *I_BaseTiccmd (void)
{
	if (!doomcon)
		return &emptycmd;

	DPMIInt (doomcon->f_0);
	return &doomcon->f_4;
}

/*
===================
=
= I_GetTime
=
= Returns time in 1/35th second tics.
=
===================
*/

//TODO implement timer
#if 0
int32_t I_GetTime (void)
{
#ifdef NOTIMER
	ticcount++;
#endif
	return (ticcount);
}
#else
#include <time.h>
static uint32_t basetime;

void I_InitBaseTime(void)
{
	basetime = clock();
}

int32_t I_GetTime (void)
{
	uint32_t ticks = clock();

	ticks -= basetime;

	return (ticks * TICRATE) / CLOCKS_PER_SEC;
}
#endif

/*
===================
=
= I_WaitVBL
=
===================
*/

void I_WaitVBL (int32_t vbls)
{
	int32_t	stat;

	if (novideo)
		return;

	while (vbls--)
	{
		do
		{
			stat = inp (STATUS_REGISTER_1);
			if (stat&8)
				break;
		} while(1);
		do
		{
			stat = inp (STATUS_REGISTER_1);
			if ((stat&8) == 0)
				break;
		} while(1);
	}
}

/*
===================
=
= I_SetPalette
=
= Palette source must use 8 bit RGB elements.
=
===================
*/

void I_SetPalette (byte *palette)
{
	int32_t	i;

	if (novideo)
		return;

	I_WaitVBL (1);
	_outbyte (PEL_WRITE_ADR, 0);
	for (i = 0; i < 768; i++)
		_outbyte (PEL_DATA, (gammatable[usegamma][*palette++])>>2);
}

/*
============================================================================

							GRAPHICS MODE

============================================================================
*/

static byte *screen, *currentscreen;
byte *destscreen;
byte *destview	__attribute__ ((externally_visible));

/*
===================
=
= I_UpdateBox
=
===================
*/

static void I_UpdateBox (int32_t x, int32_t y, int32_t width, int32_t height)
{
	int32_t		ofs;
	byte	*source;
	int16_t	*dest;
	int32_t		p,x1, x2;
	int32_t		srcdelta, destdelta;
	int32_t		wwide;

#ifdef RANGECHECK
	if (x < 0 || y < 0 || width <= 0 || height <= 0
		|| x + width > SCREENWIDTH || y + height > SCREENHEIGHT)
	{
		I_Error("Bad I_UpdateBox (%i, %i, %i, %i)", x, y, width, height);
	}
#endif

	x1 = x>>3;
	x2 = (x+width)>>3;
	wwide = x2-x1+1;

	ofs = y*SCREENWIDTH+(x1<<3);
	srcdelta = SCREENWIDTH - (wwide<<3);
	destdelta = PLANEWIDTH/2 - wwide;
	outp (SC_INDEX, SC_MAPMASK);

	for (p = 0 ; p < 4 ; p++)
	{
		outp (SC_INDEX+1, 1<<p);
		source = screens[0] + ofs + p;
		dest = (int16_t *)(destscreen + (ofs>>2));
		for (y=0 ; y<height ; y++)
		{
			for (x=wwide ; x ; x--)
			{
				*dest++ = *source + (source[4]<<8);
				source += 8;
			}
			source+=srcdelta;
			dest+=destdelta;
		}
	}
}

/*
===================
=
= I_UpdateNoBlit
=
===================
*/

void I_UpdateNoBlit(void)
{
	static int32_t oldupdatebox[4];
	static int32_t voldupdatebox[4];
	int32_t updatebox[4];

	currentscreen = destscreen;
	updatebox[BOXTOP] = (dirtybox[BOXTOP] > voldupdatebox[BOXTOP]) ?
		dirtybox[BOXTOP] : voldupdatebox[BOXTOP];
	updatebox[BOXRIGHT] = (dirtybox[BOXRIGHT] > voldupdatebox[BOXRIGHT]) ?
		dirtybox[BOXRIGHT] : voldupdatebox[BOXRIGHT];
	updatebox[BOXBOTTOM] = (dirtybox[BOXBOTTOM] < voldupdatebox[BOXBOTTOM]) ?
		dirtybox[BOXBOTTOM] : voldupdatebox[BOXBOTTOM];
	updatebox[BOXLEFT] = (dirtybox[BOXLEFT] < voldupdatebox[BOXLEFT]) ?
		dirtybox[BOXLEFT] : voldupdatebox[BOXLEFT];

	updatebox[BOXTOP] = (updatebox[BOXTOP] > oldupdatebox[BOXTOP]) ?
		updatebox[BOXTOP] : oldupdatebox[BOXTOP];
	updatebox[BOXRIGHT] = (updatebox[BOXRIGHT] > oldupdatebox[BOXRIGHT]) ?
		updatebox[BOXRIGHT] : oldupdatebox[BOXRIGHT];
	updatebox[BOXBOTTOM] = (updatebox[BOXBOTTOM] < oldupdatebox[BOXBOTTOM]) ?
		updatebox[BOXBOTTOM] : oldupdatebox[BOXBOTTOM];
	updatebox[BOXLEFT] = (updatebox[BOXLEFT] < oldupdatebox[BOXLEFT]) ?
		updatebox[BOXLEFT] : oldupdatebox[BOXLEFT];

	memcpy (voldupdatebox, oldupdatebox, sizeof(oldupdatebox));
	memcpy (oldupdatebox, dirtybox, sizeof(dirtybox));

	if (updatebox[BOXTOP] >= updatebox[BOXBOTTOM])
		I_UpdateBox (updatebox[BOXLEFT], updatebox[BOXBOTTOM],
			updatebox[BOXRIGHT] - updatebox[BOXLEFT] + 1,
			updatebox[BOXTOP] - updatebox[BOXBOTTOM] + 1);

	M_ClearBox (dirtybox);
}

/*
===================
=
= I_FinishUpdate
=
===================
*/

void I_FinishUpdate (void)
{
	static	int32_t		lasttic;
	int32_t				tics, i;

	// draws little dots on the bottom of the screen
	if (devparm)
	{
		tics = ticcount - lasttic;
		lasttic = ticcount;
		if (tics > 20) tics = 20;

		outpw (SC_INDEX, SC_MAPMASK | (1 << 8));

		for (i=0 ; i<tics ; i++)
			destscreen[ (SCREENHEIGHT-1)*PLANEWIDTH + i] = 0xff;
		for ( ; i<20 ; i++)
			destscreen[ (SCREENHEIGHT-1)*PLANEWIDTH + i] = 0x0;
	}

	// page flip
	outpw (CRTC_INDEX, CRTC_STARTHIGH+((int32_t)destscreen&0xff00));

	destscreen += 0x4000;
	if ( (int32_t)destscreen == (int32_t)(0xac000 + __djgpp_conventional_base))
		destscreen = (byte *)(0xa0000 + __djgpp_conventional_base);
}

/*
===================
=
= I_InitGraphics
=
===================
*/

void I_InitGraphics (void)
{
	if (novideo)
		return;
	grmode = true;
	regs.h.ah = 0;
	regs.h.al = 0x13;
	int386 (0x10, &regs, &regs);
	screen = currentscreen = (byte *)(0xa0000 + __djgpp_conventional_base);
	destscreen = (byte *)(0xa4000 + __djgpp_conventional_base);
	outp (SC_INDEX,SC_MEMMODE);
	outp (SC_INDEX+1,(inp(SC_INDEX+1)&~8)|4);
	outp (GC_INDEX,GC_MODE);
	outp (GC_INDEX+1,inp(GC_INDEX+1)&~0x13);
	outp (GC_INDEX,GC_MISCELLANEOUS);
	outp (GC_INDEX+1,inp(GC_INDEX+1)&~2);
	outpw (SC_INDEX,SC_MAPMASK|(15<<8));
	memset (screen, 0, 65536);
	outp (CRTC_INDEX,CRTC_UNDERLINE);
	outp (CRTC_INDEX+1,inp(CRTC_INDEX+1)&~0x40);
	outp (CRTC_INDEX,CRTC_MODE);
	outp (CRTC_INDEX+1,inp(CRTC_INDEX+1)|0x40);
	outp (GC_INDEX,GC_READMAP);
	I_SetPalette (W_CacheLumpName("PLAYPAL", PU_CACHE));
	I_InitDiskFlash ();
}

/*
===================
=
= I_ShutdownGraphics
=
===================
*/

static void I_ShutdownGraphics (void)
{
	if (*(byte *)(0x449 + __djgpp_conventional_base) == 0x13) // don't reset mode if it didn't get set
	{
		regs.h.ah = 0;
		regs.h.al = 3;
		int386 (0x10, &regs, &regs); // back to text mode
	}
}

/*
===================
=
= I_ReadScreen
=
= Reads the screen currently displayed into a linear buffer.
=
===================
*/

void I_ReadScreen (byte *scr)
{
	int32_t	p, i;
	outp (GC_INDEX,GC_READMAP);
	for (p = 0; p < 4; p++)
	{
		outp (GC_INDEX+1,p);
		for (i = 0; i < SCREENWIDTH*SCREENHEIGHT/4; i++)
			scr[i*4+p] = currentscreen[i];
	}
}


//===========================================================================

/*
===================
=
= I_StartTic
=
// called by D_DoomLoop
// called before processing each tic in a frame
// can call D_PostEvent
// asyncronous interrupt functions should maintain private ques that are
// read by the syncronous functions to be converted into events
===================
*/

/*
 OLD STARTTIC STUFF

void   I_StartTic (void)
{
	int32_t             k;
	event_t ev;


	I_ReadMouse ();

//
// keyboard events
//
	while (kbdtail < kbdhead)
	{
		k = keyboardque[kbdtail&(KBDQUESIZE-1)];

//      if (k==14)
//              I_Error ("exited");

		kbdtail++;

		// extended keyboard shift key bullshit
		if ( (k&0x7f)==KEY_RSHIFT )
		{
			if ( keyboardque[(kbdtail-2)&(KBDQUESIZE-1)]==0xe0 )
				continue;
			k &= 0x80;
			k |= KEY_RSHIFT;
		}

		if (k==0xe0)
			continue;               // special / pause keys
		if (keyboardque[(kbdtail-2)&(KBDQUESIZE-1)] == 0xe1)
			continue;                               // pause key bullshit

		if (k==0xc5 && keyboardque[(kbdtail-2)&(KBDQUESIZE-1)] == 0x9d)
		{
			ev.type = ev_keydown;
			ev.data1 = KEY_PAUSE;
			D_PostEvent (&ev);
			continue;
		}

		if (k&0x80)
			ev.type = ev_keyup;
		else
			ev.type = ev_keydown;
		k &= 0x7f;

		ev.data1 = k;
		//ev.data1 = scantokey[k];

		D_PostEvent (&ev);
	}
}
*/

#define SC_UPARROW              0x48
#define SC_DOWNARROW    0x50
#define SC_LEFTARROW            0x4b
#define SC_RIGHTARROW   0x4d

void   I_StartTic (void)
{
	int32_t             k;
	event_t ev;


	I_ReadMouse ();

//
// keyboard events
//
	while (kbdtail < kbdhead)
	{
		k = keyboardque[kbdtail&(KBDQUESIZE-1)];
		kbdtail++;

		// extended keyboard shift key bullshit
		if ( (k&0x7f)==SC_LSHIFT || (k&0x7f)==SC_RSHIFT )
		{
			if ( keyboardque[(kbdtail-2)&(KBDQUESIZE-1)]==0xe0 )
				continue;
			k &= 0x80;
			k |= SC_RSHIFT;
		}

		if (k==0xe0)
			continue;               // special / pause keys
		if (keyboardque[(kbdtail-2)&(KBDQUESIZE-1)] == 0xe1)
			continue;                               // pause key bullshit

		if (k==0xc5 && keyboardque[(kbdtail-2)&(KBDQUESIZE-1)] == 0x9d)
		{
			ev.type = ev_keydown;
			ev.data1 = KEY_PAUSE;
			D_PostEvent (&ev);
			continue;
		}

		if (k&0x80)
			ev.type = ev_keyup;
		else
			ev.type = ev_keydown;
		k &= 0x7f;
		switch (k)
		{
		case SC_UPARROW:
			ev.data1 = KEY_UPARROW;
			break;
		case SC_DOWNARROW:
			ev.data1 = KEY_DOWNARROW;
			break;
		case SC_LEFTARROW:
			ev.data1 = KEY_LEFTARROW;
			break;
		case SC_RIGHTARROW:
			ev.data1 = KEY_RIGHTARROW;
			break;
		default:
			ev.data1 = scantokey[k];
			break;
		}
		D_PostEvent (&ev);
	}

}


/*
============================================================================

					TIMER INTERRUPT

============================================================================
*/

/*
================
=
= I_TimerISR
=
================
*/

int32_t I_TimerISR (void)
{
	ticcount++;
	return 0;
}

/*
============================================================================

						KEYBOARD

============================================================================
*/

static int32_t lastpress;

/*
================
=
= I_KeyboardISR
=
================
*/

static void _interrupt I_KeyboardISR (void)
{
// Get the scan code

	keyboardque[kbdhead&(KBDQUESIZE-1)] = lastpress = _inbyte(0x60);
	kbdhead++;

// acknowledge the interrupt

	_outbyte(0x20,0x20);
}



/*
===============
=
= I_StartupKeyboard
=
===============
*/

#if defined __DJGPP__
static _go32_dpmi_seginfo oldkeyboardisr = {0}, newkeyboardisr;
#elif defined __DMC__
static unsigned oldkeyboardisroffset, oldkeyboardisrsegment;
#elif defined __CCDL__
static uint16_t oldkeyboardisrsegment, oldkeyboardisroffset = 0;
#elif defined __WATCOMC__
static void (_interrupt __far *oldkeyboardisr) () = NULL;
#endif

static void I_StartupKeyboard (void)
{
#if defined __DJGPP__
	_go32_dpmi_get_protected_mode_interrupt_vector(KEYBOARDINT, &oldkeyboardisr);

	newkeyboardisr.pm_offset = (int32_t)I_KeyboardISR;
	newkeyboardisr.pm_selector = _go32_my_cs(); 
	_go32_dpmi_allocate_iret_wrapper(&newkeyboardisr);
	_go32_dpmi_set_protected_mode_interrupt_vector(KEYBOARDINT, &newkeyboardisr);
#elif defined __DMC__
	int_getvector(KEYBOARDINT, &oldkeyboardisroffset, &oldkeyboardisrsegment);
	//FIXME int_setvector(KEYBOARDINT, FP_OFF(I_KeyboardISR), FP_SEG(I_KeyboardISR));
#elif defined __CCDL__
	struct SREGS	segregs;

	_segread(&segregs);
	dpmi_get_real_interrupt(&oldkeyboardisrsegment, &oldkeyboardisroffset, KEYBOARDINT);
	dpmi_set_protected_interrupt(KEYBOARDINT, segregs.cs, (uint32_t)I_KeyboardISR);
#elif defined __WATCOMC__
	oldkeyboardisr = _dos_getvect(KEYBOARDINT);
	_dos_setvect (0x8000 | KEYBOARDINT, I_KeyboardISR);
#else
	//TODO implement I_StartupKeyboard()
#endif
}


static void I_ShutdownKeyboard (void)
{
#if defined __DJGPP__
	if (oldkeyboardisr.size)
	{
		_go32_dpmi_set_protected_mode_interrupt_vector(KEYBOARDINT, &oldkeyboardisr);
		_go32_dpmi_free_iret_wrapper(&newkeyboardisr);
	}
#elif defined __DMC__
	//if (oldkeyboardisroffset)
	//FIXME	int_setvector (KEYBOARDINT, oldkeyboardisroffset, oldkeyboardisrsegment);
#elif defined __CCDL__
	if (oldkeyboardisroffset)
		dpmi_set_real_interrupt(KEYBOARDINT, oldkeyboardisrsegment, oldkeyboardisroffset);
#elif defined __WATCOMC__
	if (oldkeyboardisr)
		_dos_setvect (KEYBOARDINT, oldkeyboardisr);
#else
	//TODO implement I_ShutdownKeyboard()
#endif

	*(int16_t *)(0x41c + __djgpp_conventional_base) = *(int16_t *)(0x41a + __djgpp_conventional_base);      // clear bios key buffer
}



/*
============================================================================

							MOUSE

============================================================================
*/


static boolean I_ResetMouse (void)
{
	regs.h.ah = 0;                  // reset
	regs.h.al = 0;
	int386 (0x33, &regs, &regs);
	return regs.h.al != 0;
}



/*
================
=
= StartupMouse
=
================
*/

static void I_StartupMouse (void)
{
   //
   // General mouse detection
   //
	mousepresent = false;
	if ( M_CheckParm ("-nomouse") || !usemouse )
		return;

	if (I_ResetMouse ())
	{
		mousepresent = true;
		printf("Mouse: detected\n");
	} else
		printf("Mouse: not present\n");
}


/*
================
=
= ShutdownMouse
=
================
*/

static void I_ShutdownMouse (void)
{
	if (!mousepresent)
	  return;

	I_ResetMouse ();
}


/*
================
=
= I_ReadMouse
=
================
*/

static void I_ReadMouse (void)
{
	event_t ev;

//
// mouse events
//
	if (!mousepresent)
		return;

	ev.type = ev_mouse;

	regs.h.ah = 0;
	regs.h.al = 3;                               // read buttons / position
	int386 (0x33, &regs, &regs);
	ev.data1 = regs.h.bl;

	regs.h.ah = 0;
	regs.h.al = 11;                              // read counters
	int386 (0x33, &regs, &regs);
#if defined __CCDL__ || defined __WATCOMC__
	ev.data2 = (int16_t)regs.w.cx;
	ev.data3 = -(int16_t)regs.w.dx;
#else
	ev.data2 = (int16_t)regs.x.cx;
	ev.data3 = -(int16_t)regs.x.dx;
#endif

	D_PostEvent (&ev);
}

/*
============================================================================

					JOYSTICK

============================================================================
*/

static int32_t     joyxl, joyxh, joyyl, joyyh;

static boolean WaitJoyButton (void)
{
	int32_t             oldbuttons, buttons;

	oldbuttons = 0;
	do
	{
		I_WaitVBL (1);
		buttons =  ((inp(0x201) >> 4)&1)^1;
		if (buttons != oldbuttons)
		{
			oldbuttons = buttons;
			continue;
		}

		if ( (lastpress& 0x7f) == 1 )
		{
			joystickpresent = false;
			return false;
		}
	} while ( !buttons);

	do
	{
		I_WaitVBL (1);
		buttons =  ((inp(0x201) >> 4)&1)^1;
		if (buttons != oldbuttons)
		{
			oldbuttons = buttons;
			continue;
		}

		if ( (lastpress& 0x7f) == 1 )
		{
			joystickpresent = false;
			return false;
		}
	} while ( buttons);

	return true;
}



/*
===============
=
= I_StartupJoystick
=
===============
*/

static void I_StartupJoystick (void)
{
	int32_t     centerx, centery;

	joystickpresent = 0;
	if ( M_CheckParm ("-nojoy") || !usejoystick )
		return;

	if (!I_ReadJoystick ())
	{
		joystickpresent = false;
		printf ("joystick not found\n");
		return;
	}
	printf ("joystick found\n");
	joystickpresent = true;

	printf ("CENTER the joystick and press button 1:");
	if (!WaitJoyButton ())
		return;
	I_ReadJoystick ();
	centerx = joystickx;
	centery = joysticky;

	printf ("\nPush the joystick to the UPPER LEFT corner and press button 1:");
	if (!WaitJoyButton ())
		return;
	I_ReadJoystick ();
	joyxl = (centerx + joystickx)/2;
	joyyl = (centerx + joysticky)/2;

	printf ("\nPush the joystick to the LOWER RIGHT corner and press button 1:");
	if (!WaitJoyButton ())
		return;
	I_ReadJoystick ();
	joyxh = (centerx + joystickx)/2;
	joyyh = (centery + joysticky)/2;
	printf ("\n");
}

/*
===============
=
= I_StartFrame
=
===============
*/

void I_StartFrame (void)
{
	event_t ev;

//
// joystick events
//
	if (!joystickpresent)
		return;

	I_ReadJoystick ();
	ev.type = ev_joystick;
	ev.data1 =  ((inp(0x201) >> 4)&15)^15;

	if (joystickx < joyxl)
		ev.data2 = -1;
	else if (joystickx > joyxh)
		ev.data2 = 1;
	else
		ev.data2 = 0;
	if (joysticky < joyyl)
		ev.data3 = -1;
	else if (joysticky > joyyh)
		ev.data3 = 1;
	else
		ev.data3 = 0;

	D_PostEvent (&ev);
}



/*
============================================================================

					DPMI STUFF

============================================================================
*/

#if defined __WATCOMC__
#define REALSTACKSIZE   1024

static uint32_t                realstackseg;
#endif

static void DPMIInt (int32_t i)
{
#if defined __DJGPP__
	__dpmi_regs		dpmiregs;

	__dpmi_int(i, &dpmiregs);
#elif defined __CCDL__
	DPMI_REGS		dpmiregs;

	dpmi_simulate_real_interrupt(i, &dpmiregs);
#elif defined __WATCOMC__
	__dpmi_regs		dpmiregs;
	struct SREGS	segregs;

	dpmiregs.x.ss = realstackseg;
	dpmiregs.x.sp = REALSTACKSIZE-4;

	segread (&segregs);
	regs.w.ax = 0x300;
	regs.w.bx = i;
	regs.w.cx = 0;
	regs.x.edi = (uint32_t)&dpmiregs;
	segregs.es = segregs.ds;
	int386x( DPMI_INT, &regs, &regs, &segregs );
#else
	I_Error("TODO implement DPMIInt()");
#endif
}


/*
=============
=
= I_AllocLow
=
=============
*/
#if defined __WATCOMC__
static byte *I_AllocLow (int32_t length)
{
	byte    *mem;

	// DPMI call 100h allocates DOS memory
	regs.w.ax = 0x0100;          // DPMI allocate DOS memory
	regs.w.bx = (length+15) / 16;
	int386( DPMI_INT, &regs, &regs);
	if (regs.w.cflag != 0)
		I_Error ("I_AllocLow: DOS alloc of %i failed, %i free",
			length, regs.w.bx*16);


	mem = (void *) ((regs.x.eax & 0xFFFF) << 4);

	memset (mem,0,length);
	return mem;
}
#endif


/*
==============
=
= I_StartupDPMI
=
==============
*/

static void I_StartupDPMI (void)
{
#if defined __DJGPP__
	__djgpp_nearptr_enable();
#elif defined __WATCOMC__
	void _dpmi_lockregion (void * inmem, int32_t length);
	extern char __begtext;
	extern char ___Argc;

//
// allocate a decent stack for real mode ISRs
//
	realstackseg = (int32_t)I_AllocLow (REALSTACKSIZE) >> 4;

//
// lock the entire program down
//

	_dpmi_lockregion (&__begtext, &___Argc - &__begtext);
#endif
}



//===========================================================================


/*
===============
=
= I_Init
=
= hook interrupts and set graphics mode
=
===============
*/

void I_Init (void)
{
	int32_t i;

	novideo = M_CheckParm("novideo");
	i = M_CheckParm("-control");
	if (i)
	{
		doomcon = (doomcontrol_t*)atoi(myargv[i+1]);
		printf("Using external control API\n");
	}
	printf ("I_StartupDPMI\n");
	I_StartupDPMI ();
	printf ("I_StartupMouse\n");
	I_StartupMouse ();
	printf ("I_StartupJoystick\n");
	I_StartupJoystick ();
	printf ("I_StartupKeyboard\n");
	I_StartupKeyboard ();
	printf ("I_StartupSound\n");
	I_StartupSound ();
}


/*
===============
=
= I_Shutdown
=
= return to default system state
=
===============
*/

static void I_Shutdown (void)
{
	I_ShutdownGraphics ();
	I_ShutdownSound ();
	I_ShutdownTimer ();
	I_ShutdownMouse ();
	I_ShutdownKeyboard ();
}


/*
================
=
= I_Error
=
================
*/

void I_Error (char *error, ...)
{
	va_list argptr;

	D_QuitNetGame ();
	I_Shutdown ();
	va_start (argptr,error);
	vprintf (error,argptr);
	va_end (argptr);
	printf ("\n");
	exit (1);
}

/*
===============
=
= I_Quit
=
= Shuts down net game, saves defaults, prints the exit text message,
= goes to text mode, and exits.
=
===============
*/

void I_Quit (void)
{
	byte *scr;

	if (demorecording)
		G_CheckDemoStatus ();
	else
		D_QuitNetGame ();
	M_SaveDefaults ();
	scr = (byte *)W_CacheLumpName ("ENDOOM", PU_CACHE);
	I_Shutdown ();
	memcpy ((void *)(0xb8000 + __djgpp_conventional_base), scr, 80*25*2);
	regs.h.ah = 2;
	regs.h.bh = 0;
	regs.h.dl = 0;
	regs.h.dh = 23;
	int386 (0x10, &regs, &regs); // Set text pos
	printf ("\n");
	exit (0);
}

/*
===============
=
= I_ZoneBase
=
===============
*/

static int32_t I_GetLargestAvailableFreeBlockInBytes(void)
{
#if defined __DJGPP__
	return _go32_dpmi_remaining_physical_memory();
#elif defined __DMC__
	return _memmax();
#elif defined __WATCOMC__
	struct SREGS			segregs;
	__dpmi_free_mem_info	meminfo;

	regs.w.ax = 0x500;      // get memory info
	memset (&segregs,0,sizeof(segregs));
	segregs.es = FP_SEG(&meminfo);
	regs.x.edi = FP_OFF(&meminfo);
	int386x( DPMI_INT, &regs, &regs, &segregs );
	return meminfo.largest_available_free_block_in_bytes;
#else
	return 0x800000 + 0x20000; //TODO implement I_GetLargestAvailableFreeBlockInBytes()
#endif
}

byte *I_ZoneBase (int32_t *size)
{
	int32_t	heap;
	byte	*ptr;

	heap = I_GetLargestAvailableFreeBlockInBytes();
	printf ("DPMI memory: 0x%x",heap);

	do
	{
		heap -= 0x20000;                // leave 64k alone
		if (heap > 0x800000)
			heap = 0x800000;
		ptr = malloc (heap);
	} while (!ptr);

	printf (", 0x%x allocated for zone\n", heap);
	if (heap < 0x180000)
	{
		printf ("\n");
		printf ("Insufficient memory!  You need to have at least 3.7 megabytes of total\n");
#if APPVER_CHEX
		printf ("free memory available for Chex(R) Quest.  Reconfigure your CONFIG.SYS\n");
		printf ("or AUTOEXEC.BAT to load fewer device drivers or TSR's.  We recommend\n");
		printf ("creating a custom boot menu item in your CONFIG.SYS for optimum play.\n");
		printf ("Please consult your DOS manual (\"Making more memory available\") for\n");
		printf ("information on how to free up more memory for Chex(R) Quest.\n\n");
		printf ("Chex(R) Quest aborted.\n");
		exit (1);
#else
		printf ("free memory available for DOOM to execute.  Reconfigure your CONFIG.SYS\n");
		printf ("or AUTOEXEC.BAT to load fewer device drivers or TSR's.  We recommend\n");
		printf ("creating a custom boot menu item in your CONFIG.SYS for optimum DOOMing.\n");
		printf ("Please consult your DOS manual (\"Making more memory available\") for\n");
		printf ("information on how to free up more memory for DOOM.\n\n");
#if (APPVER_DOOMREV < AV_DR_DM1666E)
		I_Error ("DOOM aborted.");
#else
		printf ("DOOM aborted.\n");
		exit (1);
#endif
#endif // APPVER_CHEX
	}

	*size = heap;
	return ptr;
}

/*
=============================================================================

					DISK ICON FLASHING

=============================================================================
*/

static void I_InitDiskFlash (void)
{
	void    *pic;
	byte    *temp;

	if (M_CheckParm ("-cdrom"))
		pic = W_CacheLumpName ("STCDROM",PU_CACHE);
	else
		pic = W_CacheLumpName ("STDISK",PU_CACHE);
	temp = destscreen;
	destscreen = (byte *)(0xac000 + __djgpp_conventional_base);
	V_DrawPatchDirect (SCREENWIDTH-16,SCREENHEIGHT-16,pic);
	destscreen = temp;
}

// draw disk icon
void I_BeginRead (void)
{
	byte    *src,*dest;
	int32_t             y;

	if (!grmode)
		return;

// write through all planes
	outp (SC_INDEX,SC_MAPMASK);
	outp (SC_INDEX+1,15);
// set write mode 1
	outp (GC_INDEX,GC_MODE);
	outp (GC_INDEX+1,inp(GC_INDEX+1)|1);

// copy to backup
	src = currentscreen + 184*PLANEWIDTH + 304/4;
	dest = (byte *)(0xac000 + __djgpp_conventional_base + 184*PLANEWIDTH + 288/4);
	for (y=0 ; y<16 ; y++)
	{
		dest[0] = src[0];
		dest[1] = src[1];
		dest[2] = src[2];
		dest[3] = src[3];
		src += PLANEWIDTH;
		dest += PLANEWIDTH;
	}

// copy disk over
	dest = currentscreen + 184*PLANEWIDTH + 304/4;
	src = (byte *)(0xac000 + __djgpp_conventional_base + 184*PLANEWIDTH + 304/4);
	for (y=0 ; y<16 ; y++)
	{
		dest[0] = src[0];
		dest[1] = src[1];
		dest[2] = src[2];
		dest[3] = src[3];
		src += PLANEWIDTH;
		dest += PLANEWIDTH;
	}


// set write mode 0
	outp (GC_INDEX,GC_MODE);
	outp (GC_INDEX+1,inp(GC_INDEX+1)&~1);
}

// erase disk icon
void I_EndRead (void)
{
	byte    *src,*dest;
	int32_t             y;

	if (!grmode)
		return;

// write through all planes
	outp (SC_INDEX,SC_MAPMASK);
	outp (SC_INDEX+1,15);
// set write mode 1
	outp (GC_INDEX,GC_MODE);
	outp (GC_INDEX+1,inp(GC_INDEX+1)|1);


// copy disk over
	dest = currentscreen + 184*PLANEWIDTH + 304/4;
	src = (byte *)(0xac000 + __djgpp_conventional_base + 184*PLANEWIDTH + 288/4);
	for (y=0 ; y<16 ; y++)
	{
		dest[0] = src[0];
		dest[1] = src[1];
		dest[2] = src[2];
		dest[3] = src[3];
		src += PLANEWIDTH;
		dest += PLANEWIDTH;
	}

// set write mode 0
	outp (GC_INDEX,GC_MODE);
	outp (GC_INDEX+1,inp(GC_INDEX+1)&~1);
}



/*
============================================================================

						NETWORKING

============================================================================
*/

/* // FUCKED LINES
typedef struct
{
	char    priv[508];
} doomdata_t;
*/ // FUCKED LINES

#define DOOMCOM_ID              0x12345678l

/* // FUCKED LINES
typedef struct
{
	int32_t    id;
	int16_t   intnum;                 // DOOM executes an int to execute commands

// communication between DOOM and the driver
	int16_t   command;                // CMD_SEND or CMD_GET
	int16_t   remotenode;             // dest for send, set by get (-1 = no packet)
	int16_t   datalength;             // bytes in doomdata to be sent

// info common to all nodes
	int16_t   numnodes;               // console is allways node 0
	int16_t   ticdup;                 // 1 = no duplication, 2-5 = dup for slow nets
	int16_t   extratics;              // 1 = send a backup tic in every packet
	int16_t   deathmatch;             // 1 = deathmatch
	int16_t   savegame;               // -1 = new game, 0-5 = load savegame
	int16_t   episode;                // 1-3
	int16_t   map;                    // 1-9
	int16_t   skill;                  // 1-5

// info specific to this node
	int16_t   consoleplayer;
	int16_t   numplayers;
	int16_t   angleoffset;    // 1 = left, 0 = center, -1 = right
	int16_t   drone;                  // 1 = drone

// packet data to be sent
	doomdata_t      data;
} doomcom_t;
*/ // FUCKED LINES

extern  doomcom_t               *doomcom;

/*
====================
=
= I_InitNetwork
=
====================
*/

void I_InitNetwork (void)
{
	int32_t             i;

	i = M_CheckParm ("-net");
	if (!i)
	{
	//
	// single player game
	//
		doomcom = malloc (sizeof (*doomcom) );
		if (!doomcom)
			I_Error("malloc() in I_InitNetwork() failed");
		memset (doomcom, 0, sizeof(*doomcom) );
		netgame = false;
		doomcom->id = DOOMCOM_ID;
		doomcom->numplayers = doomcom->numnodes = 1;
		doomcom->deathmatch = false;
		doomcom->consoleplayer = 0;
		doomcom->ticdup = 1;
		doomcom->extratics = 0;
		return;
	}

	netgame = true;
	doomcom = (doomcom_t *)atoi(myargv[i+1]);
//DEBUG
doomcom->skill = startskill;
doomcom->episode = startepisode;
doomcom->map = startmap;
doomcom->deathmatch = deathmatch;

}

void I_NetCmd (void)
{
	if (!netgame)
		I_Error ("I_NetCmd when not in netgame");
	DPMIInt (doomcom->intnum);
}
