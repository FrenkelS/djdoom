;
; Copyright (C) 1993-1996 Id Software, Inc.
; Copyright (C) 2023 Frenkel Smeijers
;
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License
; as published by the Free Software Foundation; either version 2
; of the License, or (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program. If not, see <https://www.gnu.org/licenses/>.
;

cpu 386

PLANEWIDTH		equ	80

%ifidn __OUTPUT_FORMAT__,elf
section .note.GNU-stack noalloc noexec nowrite progbits
%endif

section .data

extern _loop_count 

%ifidn __OUTPUT_FORMAT__,elf
section .text
%endif
%ifidn __OUTPUT_FORMAT__, coff
section .text public class=CODE USE32
%elifidn __OUTPUT_FORMAT__, obj
section _TEXT public class=CODE USE32
%endif

global _R_ScaleColumnAsm
_R_ScaleColumnAsm:
	add edi, PLANEWIDTH * 32
%assign ROW 32
%rep 32 / 2
	lea ecx, [edx + ebx]
	shr edx, 25
	mov al, [esi + edx]
	mov al, [eax]
	mov [edi - PLANEWIDTH * ROW], al
	lea edx, [ecx + ebx]
	shr ecx, 25
	mov al, [esi + ecx]
	mov al, [eax]
	mov [edi - PLANEWIDTH * ROW + PLANEWIDTH], al
	%assign ROW ROW - 2
 %endrep
	dec dword [_loop_count]
	jns _R_ScaleColumnAsm
	ret

global _R_ScaleRowAsm
_R_ScaleRowAsm:
	add edi, 32
%assign COL 32
%rep 32 / 2
	lea ecx, [edx + ebx]
	shr edx, 26
	shld dx, cx, 6
	mov al, [esi + edx]
	mov al, [eax]
	mov [edi - COL], al
	lea edx, [ecx + ebx]
	shr ecx, 26
	shld cx, dx, 6
	mov al, [esi + ecx]
	mov al, [eax]
	mov [edi - COL + 1], al
	%assign COL COL - 2
 %endrep
	dec dword [_loop_count]
	jns _R_ScaleRowAsm
	ret
