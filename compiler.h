//
//
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

#ifndef __COMPILER__
#define __COMPILER__

#if defined __DJGPP__
//DJGPP
#include <dpmi.h>
#include <go32.h>
#include <sys/nearptr.h>

#define mkdir(x) mkdir(x,0)

//DJGPP doesn't inline inp, outp and outpw,
//but it does inline inportb, outportb and outportw
#define inp(port)			inportb(port)
#define outp(port,data)		outportb(port,data)
#define outpw(port,data)	outportw(port,data)

#define _interrupt
#define _far

#define replaceInterrupt(OldInt,NewInt,vector,handler)				\
_go32_dpmi_get_protected_mode_interrupt_vector(vector, &OldInt);	\
																	\
NewInt.pm_selector = _go32_my_cs(); 								\
NewInt.pm_offset = (int32_t)handler;								\
_go32_dpmi_allocate_iret_wrapper(&NewInt);							\
_go32_dpmi_set_protected_mode_interrupt_vector(vector, &NewInt)

#define restoreInterrupt(vector,OldInt,NewInt)						\
_go32_dpmi_set_protected_mode_interrupt_vector(vector, &OldInt);	\
_go32_dpmi_free_iret_wrapper(&NewInt)

#define _chain_intr(OldInt)		\
asm								\
(								\
	"cli \n"					\
	"pushfl \n"					\
	"lcall *%0"					\
	:							\
	: "m" (OldInt.pm_offset)	\
)



#elif defined __DMC__
//Digital Mars
#include <int.h>
#define int386 int86
#define __djgpp_conventional_base ((int32_t)_x386_zero_base_ptr)
#define __attribute__(x)

#define replaceInterrupt(OldInt,NewInt,vector,handler)	int_intercept(vector,handler,0)

#define restoreInterrupt(vector,OldInt,NewInt)	int_restore(vector)

#define _chain_intr(OldInt)	return(0)



#elif defined __CCDL__
//CC386
#include <dpmi.h>
#include <i86.h>

#define int386 _int386
#define __djgpp_conventional_base 0
#define __attribute__(x)

typedef struct
{
	uint32_t pm_offset;
	uint16_t pm_selector;
} _go32_dpmi_seginfo;

#define replaceInterrupt(OldInt,NewInt,vector,handler)								\
{																					\
	struct SREGS	segregs;														\
																					\
	_segread(&segregs);																\
	dpmi_get_protected_interrupt(&OldInt.pm_selector, &OldInt.pm_offset, vector);	\
	dpmi_set_protected_interrupt(vector, segregs.cs, (uint32_t)handler);			\
}

#define restoreInterrupt(vector,OldInt,NewInt)	dpmi_set_protected_interrupt(vector,OldInt.pm_selector,OldInt.pm_offset)

//TODO call OldInt() instead of acknowledging the interrupt
#define _chain_intr(OldInt)	\
outp(0x20,0x20);			\
return



#elif defined __WATCOMC__
//Watcom
#define __djgpp_conventional_base 0
#define __attribute__(x)

#define replaceInterrupt(OldInt,NewInt,vector,handler)	\
OldInt = _dos_getvect(vector);							\
_dos_setvect(vector, handler)

#define restoreInterrupt(vector,OldInt,NewInt)	_dos_setvect(vector,OldInt)

typedef union {
	struct {
		uint32_t	edi, esi, ebp, reserved, ebx, edx, ecx, eax;
	} d;
	struct {
		uint16_t	di, di_hi;
		uint16_t	si, si_hi;
		uint16_t	bp, bp_hi;
		uint16_t	res, res_hi;
		uint16_t	bx, bx_hi;
		uint16_t	dx, dx_hi;
		uint16_t	cx, cx_hi;
		uint16_t	ax, ax_hi;
		uint16_t	flags, es, ds, fs, gs, ip, cs, sp, ss;
	} x;
} __dpmi_regs;

typedef struct {
	uint32_t	largest_available_free_block_in_bytes;
	uint32_t	maximum_unlocked_page_allocation_in_pages;
	uint32_t	maximum_locked_page_allocation_in_pages;
	uint32_t	linear_address_space_size_in_pages;
	uint32_t	total_number_of_unlocked_pages;
	uint32_t	total_number_of_free_pages;
	uint32_t	total_number_of_physical_pages;
	uint32_t	free_linear_address_space_in_pages;
	uint32_t	size_of_paging_file_partition_in_pages;
	uint32_t	reserved[3];
} __dpmi_free_mem_info;

#if !defined C_ONLY
#pragma aux FixedMul =	\
	"imul ecx",			\
	"shrd eax, edx, 16"	\
	value	[eax]		\
	parm	[eax] [ecx] \
	modify	[edx]

typedef int32_t fixed_t;
fixed_t	FixedDiv2 (fixed_t a, fixed_t b);
#pragma aux FixedDiv2 =		\
	"cdq",					\
	"shld edx, eax, 16",	\
	"shl eax, 16",			\
	"idiv ecx"				\
	value	[eax]			\
	parm	[eax] [ecx] 	\
	modify	[edx]
#endif



#endif

#endif
