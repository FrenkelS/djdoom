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

SC_INDEX		equ	0x3c4

extern	_dc_colormap
extern	_dc_x
extern	_dc_yl
extern	_dc_yh
extern	_dc_iscale
extern	_dc_texturemid
extern	_dc_source


extern	_ds_y
extern	_ds_x1
extern	_ds_x2
extern	_ds_colormap
extern	_ds_xfrac
extern	_ds_yfrac
extern	_ds_xstep
extern	_ds_ystep
extern	_ds_source

extern	_destview
extern	_centery


;============================================================================
;
; unwound vertical scaling code
;
; eax   light table pointer, 0 lowbyte overwritten
; ebx   all 0, low byte overwritten
; ecx   fractional step value
; edx   fractional scale value
; esi   start of source pixels
; edi   bottom pixel in screenbuffer to blit into
;
; ebx should be set to 0 0 0 dh to feed the pipeline
;
; The graphics wrap vertically at 128 pixels
;============================================================================

%ifidn __OUTPUT_FORMAT__, coff
section .bss public class=DATA USE32
%elifidn __OUTPUT_FORMAT__, obj
section _BSS public class=DATA USE32
%endif

loopcount	resd 1
pixelcount	resd 1

;=================================


%ifidn __OUTPUT_FORMAT__, coff
section .text public class=CODE USE32
%elifidn __OUTPUT_FORMAT__, obj
section _TEXT public class=CODE USE32
%endif

;================
;
; R_DrawColumn
;
;================

global   _R_DrawColumnLow
_R_DrawColumnLow:
	pushad
	mov		ebp,[_dc_yl]
	cmp		ebp,[_dc_yh]
	jg		done
	lea		edi,[ebp+ebp*4]
	shl		edi,4
	mov		ebx,[_dc_x]
	mov		ecx,ebx
	shr		ebx,1
	add		edi,ebx
	add		edi,[_destview]
	and		ecx,1
	shl		ecx,1
	mov		eax,3
	shl		eax,cl
	mov		edx,SC_INDEX+1
	out		dx,al
	jmp		cdraw

global   _R_DrawColumn
_R_DrawColumn:
	pushad
	mov		ebp,[_dc_yl]
	cmp		ebp,[_dc_yh]
	jg		done
	lea		edi,[ebp+ebp*4]
	shl		edi,4
	mov		ebx,[_dc_x]
	mov		ecx,ebx
	shr		ebx,2
	add		edi,ebx
	add		edi,[_destview]
	and		ecx,3
	mov		eax,1
	shl		eax,cl
	mov		edx,SC_INDEX+1
	out		dx,al
	
cdraw:
	mov		eax,[_dc_yh]
	inc		eax
	sub		eax,ebp						; pixel count
	mov		[pixelcount],eax			; save for final pixel
	js		done						; nothing to scale
	shr		eax,1						; double pixel count
	mov		[loopcount],eax
	mov		ecx,[_dc_iscale]
	mov		eax,[_centery]
	sub		eax,ebp
	imul	ecx
	mov		ebp,[_dc_texturemid]
	sub		ebp,eax
	shl		ebp,9						; 7 significant bits, 25 frac
	mov		esi,[_dc_source]
	mov		ebx,[_dc_iscale]
	shl		ebx,9
	mov		eax,patch1+2				; convice tasm to modify code...
	mov		[eax],ebx
	mov		eax,patch2+2				; convice tasm to modify code...
	mov		[eax],ebx

; eax		aligned colormap
; ebx		aligned colormap
; ecx,edx	scratch
; esi		virtual source
; edi		moving destination pointer
; ebp		frac

	mov		ecx,ebp						; begin calculating first pixel
	add		ebp,ebx						; advance frac pointer
	shr		ecx,25						; finish calculation for first pixel
	mov		edx,ebp						; begin calculating second pixel
	add		ebp,ebx						; advance frac pointer
	shr		edx,25						; finish calculation for second pixel
	mov		eax,[_dc_colormap]
	mov		ebx,eax
	mov		al,[esi+ecx]				; get first pixel
	mov		bl,[esi+edx]				; get second pixel
	mov		al,[eax]					; color translate first pixel
	mov		bl,[ebx]					; color translate second pixel

	test	[pixelcount],dword 0fffffffeh
	jnz		doubleloop					; at least two pixels to map
	jmp		checklast

doubleloop:
	mov		ecx,ebp						; begin calculating third pixel
patch1:
	add		ebp,12345678h				; advance frac pointer
	mov		[edi],al					; write first pixel
	shr		ecx,25						; finish calculation for third pixel
	mov		edx,ebp						; begin calculating fourth pixel
patch2:
	add		ebp,12345678h				; advance frac pointer
	mov		[edi+PLANEWIDTH],bl			; write second pixel
	shr		edx,25						; finish calculation for fourth pixel
	mov		al,[esi+ecx]				; get third pixel
	add		edi,PLANEWIDTH*2			; advance to third pixel destination
	mov		bl,[esi+edx]				; get fourth pixel
	dec		dword [loopcount]			; done with loop?
	mov		al,[eax]					; color translate third pixel
	mov		bl,[ebx]					; color translate fourth pixel
	jnz		doubleloop
checklast:
	test	[pixelcount],dword 1
	jz		done
	mov		[edi],al

done:
	popad
	ret


;============================================================================
;
; unwound horizontal texture mapping code
;
; eax   lighttable
; ebx   scratch register
; ecx   position 6.10 bits x, 6.10 bits y
; edx   step 6.10 bits x, 6.10 bits y
; esi   start of block
; edi   dest
; ebp   fff to mask bx
;
; ebp should by preset from ebx / ecx before calling
;============================================================================

%ifidn __OUTPUT_FORMAT__, coff
section .bss
%elifidn __OUTPUT_FORMAT__, obj
section _BSS
%endif

dest		resd 1
endplane	resd 1
curplane	resd 1
frac		resd 1
fracstep	resd 1
fracpstep	resd 1
curx		resd 1
curpx		resd 1
endpx		resd 1


%ifidn __OUTPUT_FORMAT__, coff
section .text
%elifidn __OUTPUT_FORMAT__, obj
section _TEXT
%endif

;================
;
; R_DrawSpan
;
; Horizontal texture mapping
;
;================

global   _R_DrawSpan
_R_DrawSpan:
	pushad

	mov		eax,[_ds_x1]
	mov		[curx],eax
	mov		ebx,eax
	and		ebx,3
	mov		[endplane],ebx
	mov		[curplane],ebx
	shr		eax,2
	mov		ebp,[_ds_y]
	lea		edi,[ebp+ebp*4]
	shl		edi,4
	add		edi,eax
	add		edi,[_destview]
	mov		[dest],edi

	mov		ebx,[_ds_xfrac]
	shl		ebx,10
	and		ebx,0ffff0000h
	mov		eax,[_ds_yfrac]
	shr		eax,6
	and		eax,0ffffh
	or		ebx,eax

	mov		[frac],ebx

	mov		ebx,[_ds_xstep]
	shl		ebx,10
	and		ebx,0ffff0000h
	mov		eax,[_ds_ystep]
	shr		eax,6
	and		eax,0ffffh
	or		ebx,eax
	
	mov		[fracstep],ebx

	shl		ebx,2
	mov		[fracpstep],ebx
	mov		eax,hpatch1+2
	mov		[eax],ebx
	mov		eax,hpatch2+2
	mov		[eax],ebx
	mov		ecx,[curplane]
hplane:
	mov		eax,1
	shl		eax,cl
	mov		edx,SC_INDEX+1
	out		dx,al
	mov		eax,[_ds_x2]
	cmp		eax,[curx]
	jb		hdone
	sub		eax,[curplane]
	js		hdoneplane
	shr		eax,2
	mov		[endpx],eax
	dec		eax
	js		hfillone
	shr		eax,1
	mov		ebx,[curx]
	shr		ebx,2
	cmp		ebx,[endpx]
	jz		hfillone
	mov		[curpx],ebx
	inc		ebx
	shr		ebx,1
	inc		eax
	sub		eax,ebx
	js		hdoneplane
	mov		[loopcount],eax
	mov		eax,[_ds_colormap]
	mov		ebx,eax
	mov		esi,[_ds_source]
	mov		edi,[dest]
	mov		ebp,[frac]
	test	[curpx],dword 1
	jz		hfill
	shld	ecx,ebp,22
	shld	ecx,ebp,6
	add		ebp,[fracpstep]
	and		ecx,0fffh
	mov		al,[esi+ecx]
	mov		dl,[eax]
	mov		[edi],dl
	inc		edi
	jz		hdoneplane
hfill:
	shld	ecx,ebp,22
	shld	ecx,ebp,6
	add		ebp,[fracpstep]
	and		ecx,0fffh
	shld	edx,ebp,22
	shld	edx,ebp,6
	add		ebp,[fracpstep]
	and		edx,0fffh
	mov		al,[esi+ecx]
	mov		bl,[esi+edx]
	mov		dl,[eax]
	test	[loopcount],dword 0ffffffffh
	jnz		hdoubleloop
	jmp		hchecklast
hfillone:
	mov		eax,[_ds_colormap]
	mov		esi,[_ds_source]
	mov		edi,[dest]
	mov		ebp,[frac]
	shld	ecx,ebp,22
	shld	ecx,ebp,6
	and		ecx,0fffh
	mov		al,[esi+ecx]
	mov		dl,[eax]
	mov		[edi],dl
	jmp		hdoneplane

hdoubleloop:
	shld	ecx,ebp,22
	mov		dh,[ebx]
	shld	ecx,ebp,6
hpatch1:
	add		ebp,12345678h
	and		ecx,0fffh
	mov		[edi],dx
	shld	edx,ebp,22
	add		edi,2
	shld	edx,ebp,6
hpatch2:
	add		ebp,12345678h
	and		edx,0fffh
	mov		al,[esi+ecx]
	mov		bl,[esi+edx]
	dec		dword [loopcount]
	mov		dl,[eax]
	jnz		hdoubleloop
hchecklast:
	test	[endpx],dword 1
	jnz		hdoneplane
	mov		[edi],dl
hdoneplane:
	mov		ecx,[curplane]
	inc		ecx
	and		ecx,3
	jnz		hskip
	inc		dword [dest]
hskip:
	cmp		ecx,[endplane]
	jz		hdone
	mov		[curplane],ecx
	mov		ebx,[frac]
	add		ebx,[fracstep]
	mov		[frac],ebx
	inc		dword [curx]
	jmp		hplane
hdone:
	popad
	ret


global   _R_DrawSpanLow
_R_DrawSpanLow:
	pushad
	mov		eax,[_ds_x1]
	mov		[curx],eax
	mov 	ebx,eax
	and		ebx,1
	mov		[endplane],ebx
	mov		[curplane],ebx
	shr		eax,1
	mov		ebp,[_ds_y]
	lea		edi,[ebp+ebp*4]
	shl		edi,4
	add		edi,eax
	add		edi,[_destview]
	mov		[dest],edi
	
	mov		ebx,[_ds_xfrac]
	shl		ebx,10
	and		ebx,0ffff0000h
	mov		eax,[_ds_yfrac]
	shr		eax,6
	and		eax,0ffffh
	or		ebx,eax
	
	mov		[frac],ebx
	
	mov		ebx,[_ds_xstep]
	shl		ebx,10
	and		ebx,0ffff0000h
	mov		eax,[_ds_ystep]
	shr		eax,6
	and		eax,0ffffh
	or		ebx,eax
	
	mov		[fracstep],ebx
	
	shl		ebx,1
	mov		[fracpstep],ebx
	mov		eax,lpatch1+2
	mov		[eax],ebx
	mov		eax,lpatch2+2
	mov		[eax],ebx
	mov		ecx,[curplane]
lplane:
	mov		eax,3
	shl		eax,cl
	shl		eax,cl
	mov		edx,SC_INDEX+1
	out		dx,al
	mov		eax,[_ds_x2]
	cmp		eax,[curx]
	jb		ldone
	sub		eax,[curplane]
	js		ldoneplane
	shr		eax,1
	mov		[endpx],eax
	dec		eax
	js		lfillone
	shr		eax,1
	mov		ebx,[curx]
	shr		ebx,1
	cmp		ebx,[endpx]
	jz		lfillone
	mov		[curpx],ebx
	inc		ebx
	shr		ebx,1
	inc		eax
	sub		eax,ebx
	js		ldoneplane
	mov		[loopcount],eax
	mov		eax,[_ds_colormap]
	mov		ebx,eax
	mov		esi,[_ds_source]
	mov		edi,[dest]
	mov		ebp,[frac]
	test	[curpx],dword 1
	jz		lfill
	shld	ecx,ebp,22
	shld	ecx,ebp,6
	add		ebp,[fracpstep]
	and		ecx,0fffh
	mov		al,[esi+ecx]
	mov		dl,[eax]
	mov		[edi],dl
	inc		edi
	jz		ldoneplane
lfill:
	shld	ecx,ebp,22
	shld	ecx,ebp,6
	add		ebp,[fracpstep]
	and		ecx,0fffh
	shld	edx,ebp,22
	shld	edx,ebp,6
	add		ebp,[fracpstep]
	and		edx,0fffh
	mov		al,[esi+ecx]
	mov		bl,[esi+edx]
	mov		dl,[eax]
	test	[loopcount],dword 0ffffffffh
	jnz		ldoubleloop
	jmp		lchecklast
lfillone:
	mov		eax,[_ds_colormap]
	mov		esi,[_ds_source]
	mov		edi,[dest]
	mov		ebp,[frac]
	shld	ecx,ebp,22
	shld	ecx,ebp,6
	and		ecx,0fffh
	mov		al,[esi+ecx]
	mov		dl,[eax]
	mov		[edi],dl
	jmp		ldoneplane

ldoubleloop:
	shld	ecx,ebp,22
	mov		dh,[ebx]
	shld	ecx,ebp,6
lpatch1:
	add		ebp,12345678h
	and		ecx,0fffh
	mov		[edi],dx
	shld	edx,ebp,22
	add		edi,2
	shld	edx,ebp,6
lpatch2:
	add		ebp,12345678h
	and		edx,0fffh
	mov		al,[esi+ecx]
	mov		bl,[esi+edx]
	dec		dword [loopcount]
	mov		dl,[eax]
	jnz		ldoubleloop
lchecklast:
	test	[endpx],dword 1
	jnz		ldoneplane
	mov		[edi],dl
ldoneplane:
	mov		ecx,[curplane]
	inc		ecx
	and		ecx,1
	jnz		lskip
	inc		dword [dest]
lskip:
	cmp		ecx,[endplane]
	jz		ldone
	mov		[curplane],ecx
	mov		ebx,[frac]
	add		ebx,[fracstep]
	mov		[frac],ebx
	inc		dword [curx]
	jmp		lplane
ldone:
	popad
	ret
