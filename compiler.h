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

#define _interrupt
#define mkdir(x) mkdir(x,0)



#elif defined __DMC__
//Digital Mars
#include <int.h>
#define int386 int86
#define __djgpp_conventional_base ((byte*)_x386_zero_base_ptr)



#elif defined __CCDL__
//CC386
#include <dpmi.h>
#include <i86.h>

#define int386 _int386
#define __djgpp_conventional_base 0



#elif defined __WATCOMC__
//Watcom
#define __djgpp_conventional_base 0

typedef union {
	struct {
		unsigned		edi, esi, ebp, reserved, ebx, edx, ecx, eax;
	} d;
	struct {
		unsigned short	di, di_hi;
		unsigned short	si, si_hi;
		unsigned short	bp, bp_hi;
		unsigned short	res, res_hi;
		unsigned short	bx, bx_hi;
		unsigned short	dx, dx_hi;
		unsigned short	cx, cx_hi;
		unsigned short	ax, ax_hi;
		unsigned short	flags, es, ds, fs, gs, ip, cs, sp, ss;
	} x;
} __dpmi_regs;

typedef struct {
	unsigned long	largest_available_free_block_in_bytes;
	unsigned long	maximum_unlocked_page_allocation_in_pages;
	unsigned long	maximum_locked_page_allocation_in_pages;
	unsigned long	linear_address_space_size_in_pages;
	unsigned long	total_number_of_unlocked_pages;
	unsigned long	total_number_of_free_pages;
	unsigned long	total_number_of_physical_pages;
	unsigned long	free_linear_address_space_in_pages;
	unsigned long	size_of_paging_file_partition_in_pages;
	unsigned long	reserved[3];
} __dpmi_free_mem_info;

#if !defined C_ONLY
#pragma aux FixedMul =	\
	"imul ebx",			\
	"shrd eax,edx,16"	\
	value	[eax]		\
	parm	[eax] [ebx] \
	modify	[edx]

#pragma aux FixedDiv2 =	\
	"cdq",				\
	"shld edx,eax,16",	\
	"sal eax,16",		\
	"idiv ebx"			\
	value	[eax]		\
	parm	[eax] [ebx] \
	modify	[edx]
#endif



#endif

#endif
