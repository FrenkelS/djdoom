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

#include <dos.h>
#include <conio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "DoomDef.h"
#include "R_local.h"

#if defined __DJGPP__
#include <dpmi.h>
#include <go32.h>
#endif

extern int _wp1, _wp2;

#define DPMI_INT 0x31
//#define NOTIMER

void I_StartupNet (void);
void I_ShutdownNet (void);

void I_StartupSound (void);
void I_ShutdownSound (void);

void I_ShutdownTimer (void);

#if defined __WATCOMC__
typedef union {
  struct {
	unsigned        edi, esi, ebp, reserved, ebx, edx, ecx, eax;
  } d;
  struct {
    unsigned short  di, di_hi;
    unsigned short  si, si_hi;
    unsigned short  bp, bp_hi;
    unsigned short  res, res_hi;
    unsigned short  bx, bx_hi;
    unsigned short  dx, dx_hi;
    unsigned short  cx, cx_hi;
    unsigned short  ax, ax_hi;
	unsigned short  flags, es, ds, fs, gs, ip, cs, sp, ss;
  } x;
} __dpmi_regs;
#endif

static __dpmi_regs      dpmiregs;

static void DPMIInt (int i);

#if defined __WATCOMC__
void _dpmi_lockregion (void * inmem, int length);
#endif

static void I_ReadMouse (void);
static void I_InitDiskFlash (void);

extern  int     usemouse, usejoystick;


/*
=============================================================================

							CONSTANTS

=============================================================================
*/

#define SC_INDEX                0x3C4
#define SC_RESET                0
#define SC_CLOCK                1
#define SC_MAPMASK              2
#define SC_CHARMAP              3
#define SC_MEMMODE              4

#define CRTC_INDEX              0x3D4
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


#define GC_INDEX                0x3CE
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
static unsigned        joystickx, joysticky;
boolean I_ReadJoystick (void)	// returns false if not connected
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

int             ticcount;

// REGS stuff used for int calls
static union REGS regs;
#if defined __WATCOMC__
static struct SREGS segregs;
#endif

static boolean novideo; // if true, stay in text mode for debugging

#define KBDQUESIZE 32
static byte keyboardque[KBDQUESIZE];
static int kbdtail, kbdhead;

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
	int f_0; // interrupt
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
int I_GetTime (void)
{
#ifdef NOTIMER
	ticcount++;
#endif
	return (ticcount);
}
#else
#include <time.h>
static unsigned int basetime;

void I_InitBaseTime(void)
{
	basetime = clock();
}

int I_GetTime (void)
{
	unsigned int ticks = clock();

	ticks -= basetime;

	return (ticks * TICRATE) / CLOCKS_PER_SEC;
}
#endif

/*
===================
=
= I_ColorBorder
=
===================
*/

void I_ColorBorder (void)
{
	int	i;

	I_WaitVBL (1);
	_outbyte (PEL_WRITE_ADR, 0);
	for (i = 0; i < 3; i++)
	{
		_outbyte (PEL_DATA, 63);
	}
}

/*
===================
=
= I_UnColorBorder
=
===================
*/

void I_UnColorBorder (void)
{
	int	i;

	I_WaitVBL (1);
	_outbyte (PEL_WRITE_ADR, 0);
	for (i = 0; i < 3; i++)
	{
		_outbyte (PEL_DATA, 0);
	}
}

/*
===================
=
= I_WaitVBL
=
===================
*/

void I_WaitVBL (int vbls)
{
	int	stat;

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
	int	i;

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
byte *destscreen, *destview;

/*
===================
=
= I_UpdateBox
=
===================
*/

static void I_UpdateBox (int x, int y, int width, int height)
{
	int		ofs;
	byte	*source;
	short	*dest;
	int		p,x1, x2;
	int		srcdelta, destdelta;
	int		wwide;

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
		dest = (short *)(destscreen + (ofs>>2));
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
	static int oldupdatebox[4];
	static int voldupdatebox[4];
	int updatebox[4];

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
	static	int		lasttic;
	int				tics, i;

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
	outpw (CRTC_INDEX, CRTC_STARTHIGH+((int)destscreen&0xff00));

	destscreen += 0x4000;
	if ( (int)destscreen == 0xac000 + __djgpp_conventional_base)
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
	regs.w.ax = 0x13;
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
		regs.w.ax = 3;
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
	int	p, i;
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
	int             k;
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
	int             k;
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


void   I_ReadKeys (void)
{
	int             k;

	while (1)
	{
	   while (kbdtail < kbdhead)
	   {
		   k = keyboardque[kbdtail&(KBDQUESIZE-1)];
		   kbdtail++;
		   printf ("0x%x\n",k);
		   if (k == 1)
			   I_Quit ();
	   }
	}
}

/*
============================================================================

					TIMER INTERRUPT

============================================================================
*/

void I_ColorBlack (int r, int g, int b)
{
_outbyte (PEL_WRITE_ADR,0);
_outbyte(PEL_DATA,r);
_outbyte(PEL_DATA,g);
_outbyte(PEL_DATA,b);
}


/*
================
=
= I_TimerISR
=
================
*/

void I_TimerISR (void)
{
	ticcount++;
}

/*
============================================================================

						KEYBOARD

============================================================================
*/

#if defined __DJGPP__
static _go32_dpmi_seginfo oldkeyboardisr, newkeyboardisr;
#elif defined __WATCOMC__
static void (__interrupt __far *oldkeyboardisr) () = NULL;
#endif

static int lastpress;

/*
================
=
= I_KeyboardISR
=
================
*/

#if defined __DJGPP__
static void I_KeyboardISR (void)
#elif defined __WATCOMC__
static void __interrupt I_KeyboardISR (void)
#endif
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

static void I_StartupKeyboard (void)
{
#if defined __DJGPP__
	_go32_dpmi_get_protected_mode_interrupt_vector(KEYBOARDINT, &oldkeyboardisr);

	newkeyboardisr.pm_offset = (int)I_KeyboardISR;
	newkeyboardisr.pm_selector = _go32_my_cs(); 
	_go32_dpmi_allocate_iret_wrapper(&newkeyboardisr);
	_go32_dpmi_set_protected_mode_interrupt_vector(KEYBOARDINT, &newkeyboardisr);
#elif defined __WATCOMC__
	oldkeyboardisr = _dos_getvect(KEYBOARDINT);
	_dos_setvect (0x8000 | KEYBOARDINT, I_KeyboardISR);
#endif
}


static void I_ShutdownKeyboard (void)
{
#if defined __DJGPP__
	_go32_dpmi_set_protected_mode_interrupt_vector(KEYBOARDINT, &oldkeyboardisr);
	_go32_dpmi_free_iret_wrapper(&newkeyboardisr);
#elif defined __WATCOMC__
	if (oldkeyboardisr)
		_dos_setvect (KEYBOARDINT, oldkeyboardisr);
#endif

	*(short *)(0x41c + __djgpp_conventional_base) = *(short *)(0x41a + __djgpp_conventional_base);      // clear bios key buffer
}



/*
============================================================================

							MOUSE

============================================================================
*/


int I_ResetMouse (void)
{
	regs.w.ax = 0;                  // reset
	int386 (0x33, &regs, &regs);
	return regs.w.ax;
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
	mousepresent = 0;
	if ( M_CheckParm ("-nomouse") || !usemouse )
		return;

	if (I_ResetMouse () != 0xffff)
	{
		printf("Mouse: not present\n");
		return;
	}
	printf("Mouse: detected\n");

	mousepresent = 1;
}


/*
================
=
= ShutdownMouse
=
================
*/

void I_ShutdownMouse (void)
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

	memset (&dpmiregs,0,sizeof(dpmiregs));
	dpmiregs.d.eax = 3;                               // read buttons / position
	DPMIInt (0x33);
	ev.data1 = dpmiregs.d.ebx;

	dpmiregs.d.eax = 11;                              // read counters
	DPMIInt (0x33);
	ev.data2 = (short)dpmiregs.d.ecx;
	ev.data3 = -(short)dpmiregs.d.edx;

	D_PostEvent (&ev);
}

/*
============================================================================

					JOYSTICK

============================================================================
*/

static int     joyxl, joyxh, joyyl, joyyh;

boolean WaitJoyButton (void)
{
	int             oldbuttons, buttons;

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

int             basejoyx, basejoyy;

static void I_StartupJoystick (void)
{
	int     centerx, centery;

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

static unsigned                realstackseg;
#endif

static void DPMIInt (int i)
{
#if defined __DJGPP__
	__dpmi_int(i, &dpmiregs);
#elif defined __WATCOMC__
	dpmiregs.x.ss = realstackseg;
	dpmiregs.x.sp = REALSTACKSIZE-4;

	segread (&segregs);
	regs.w.ax = 0x300;
	regs.w.bx = i;
	regs.w.cx = 0;
	regs.x.edi = (unsigned)&dpmiregs;
	segregs.es = segregs.ds;
	int386x( DPMI_INT, &regs, &regs, &segregs );
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
static byte *I_AllocLow (int length)
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
#if defined __WATCOMC__
	extern char __begtext;
	extern char ___Argc;

//
// allocate a decent stack for real mode ISRs
//
	realstackseg = (int)I_AllocLow (1024) >> 4;

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
	int i;
	extern void I_StartupTimer(void);

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

void I_Shutdown (void)
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
	regs.w.ax = 0x0200;
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

#if defined __WATCOMC__
typedef struct {
  unsigned long largest_available_free_block_in_bytes;
  unsigned long maximum_unlocked_page_allocation_in_pages;
  unsigned long maximum_locked_page_allocation_in_pages;
  unsigned long linear_address_space_size_in_pages;
  unsigned long total_number_of_unlocked_pages;
  unsigned long total_number_of_free_pages;
  unsigned long total_number_of_physical_pages;
  unsigned long free_linear_address_space_in_pages;
  unsigned long size_of_paging_file_partition_in_pages;
  unsigned long reserved[3];
} __dpmi_free_mem_info;
#endif

byte *I_ZoneBase (int *size)
{
	__dpmi_free_mem_info             meminfo;
	int             heap;
	byte                    *ptr;

#if defined __DJGPP__
	__djgpp_nearptr_enable();
	__dpmi_get_free_memory_information(&meminfo);
#elif defined __WATCOMC__
	regs.w.ax = 0x500;      // get memory info
	memset (&segregs,0,sizeof(segregs));
	segregs.es = FP_SEG(&meminfo);
	regs.x.edi = FP_OFF(&meminfo);
	int386x( DPMI_INT, &regs, &regs, &segregs );
#endif

	heap = meminfo.largest_available_free_block_in_bytes;
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
	V_DrawPatchDirect (SCREENWIDTH-16,SCREENHEIGHT-16,0,pic);
	destscreen = temp;
}

// draw disk icon
void I_BeginRead (void)
{
	byte    *src,*dest;
	int             y;

	if (!grmode)
		return;

// write through all planes
	outp (SC_INDEX,SC_MAPMASK);
	outp (SC_INDEX+1,15);
// set write mode 1
	outp (GC_INDEX,GC_MODE);
	outp (GC_INDEX+1,inp(GC_INDEX+1)|1);

// copy to backup
	src = currentscreen + 184*80 + 304/4;
	dest = (byte *)(0xac000 + __djgpp_conventional_base + 184*80 + 288/4);
	for (y=0 ; y<16 ; y++)
	{
		dest[0] = src[0];
		dest[1] = src[1];
		dest[2] = src[2];
		dest[3] = src[3];
		src += 80;
		dest += 80;
	}

// copy disk over
	dest = currentscreen + 184*80 + 304/4;
	src = (byte *)(0xac000 + __djgpp_conventional_base + 184*80 + 304/4);
	for (y=0 ; y<16 ; y++)
	{
		dest[0] = src[0];
		dest[1] = src[1];
		dest[2] = src[2];
		dest[3] = src[3];
		src += 80;
		dest += 80;
	}


// set write mode 0
	outp (GC_INDEX,GC_MODE);
	outp (GC_INDEX+1,inp(GC_INDEX+1)&~1);
}

// erase disk icon
void I_EndRead (void)
{
	byte    *src,*dest;
	int             y;

	if (!grmode)
		return;

// write through all planes
	outp (SC_INDEX,SC_MAPMASK);
	outp (SC_INDEX+1,15);
// set write mode 1
	outp (GC_INDEX,GC_MODE);
	outp (GC_INDEX+1,inp(GC_INDEX+1)|1);


// copy disk over
	dest = currentscreen + 184*80 + 304/4;
	src = (byte *)(0xac000 + __djgpp_conventional_base + 184*80 + 288/4);
	for (y=0 ; y<16 ; y++)
	{
		dest[0] = src[0];
		dest[1] = src[1];
		dest[2] = src[2];
		dest[3] = src[3];
		src += 80;
		dest += 80;
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
	long    id;
	short   intnum;                 // DOOM executes an int to execute commands

// communication between DOOM and the driver
	short   command;                // CMD_SEND or CMD_GET
	short   remotenode;             // dest for send, set by get (-1 = no packet)
	short   datalength;             // bytes in doomdata to be sent

// info common to all nodes
	short   numnodes;               // console is allways node 0
	short   ticdup;                 // 1 = no duplication, 2-5 = dup for slow nets
	short   extratics;              // 1 = send a backup tic in every packet
	short   deathmatch;             // 1 = deathmatch
	short   savegame;               // -1 = new game, 0-5 = load savegame
	short   episode;                // 1-3
	short   map;                    // 1-9
	short   skill;                  // 1-5

// info specific to this node
	short   consoleplayer;
	short   numplayers;
	short   angleoffset;    // 1 = left, 0 = center, -1 = right
	short   drone;                  // 1 = drone

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
	int             i;

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
