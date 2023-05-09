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

// Z_zone.c

#include "doomdef.h"

/*
==============================================================================

						ZONE MEMORY ALLOCATION

There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

It is of no value to free a cachable block, because it will get overwritten
automatically if needed

==============================================================================
*/

#define	ZONEID	0x1d4a11

typedef struct
{
	int32_t		size;		// total bytes malloced, including header
	memblock_t	blocklist;		// start / end cap for linked list
	memblock_t	*rover;
} memzone_t;



static memzone_t *mainzone;

/*
========================
=
= Z_Init
=
========================
*/

void Z_Init (void)
{
	memblock_t	*block;
	int32_t		size;

	mainzone = (memzone_t *)I_ZoneBase (&size);
	mainzone->size = size;

// set the entire zone to one free block

	mainzone->blocklist.next = mainzone->blocklist.prev = block =
		(memblock_t *)( (byte *)mainzone + sizeof(memzone_t) );
	mainzone->blocklist.user = (void *)mainzone;
	mainzone->blocklist.tag = PU_STATIC;
	mainzone->rover = block;
	
	block->prev = block->next = &mainzone->blocklist;
	block->user = NULL;	// free block
	block->size = mainzone->size - sizeof(memzone_t);
}


/*
========================
=
= Z_Free
=
========================
*/

void Z_Free (void *ptr)
{
	memblock_t	*block, *other;
	
	block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID)
		I_Error ("Z_Free: freed a pointer without ZONEID");
		
	if (block->user > (void **)0x100)	// smaller values are not pointers
		*block->user = 0;		// clear the user's mark
	block->user = NULL;	// mark as free
	block->tag = 0;
	block->id = 0;
	
	other = block->prev;
	if (!other->user)
	{	// merge with previous free block
		other->size += block->size;
		other->next = block->next;
		other->next->prev = other;
		if (block == mainzone->rover)
			mainzone->rover = other;
		block = other;
	}
	
	other = block->next;
	if (!other->user)
	{	// merge the next free block onto the end
		block->size += other->size;
		block->next = other->next;
		block->next->prev = block;
		if (other == mainzone->rover)
			mainzone->rover = block;
	}
}


/*
========================
=
= Z_Malloc
=
= You can pass a NULL user if the tag is < PU_PURGELEVEL
========================
*/

#define MINFRAGMENT	64

void *Z_Malloc (int32_t size, int32_t tag, void *user)
{
	int32_t		extra;
	memblock_t	*start, *rover, *new, *base;

	size = (size + 3) & ~3;
//
// scan through the block list looking for the first free block
// of sufficient size, throwing out any purgable blocks along the way
//
	size += sizeof(memblock_t);	// account for size of block header
	
	
//
// if there is a free block behind the rover, back up over them
//
	base = mainzone->rover;
	if (!base->prev->user)
		base = base->prev;
	
	rover = base;
	start = base->prev;
	
	do
	{
		if (rover == start)		// scanned all the way around the list
			I_Error("Z_Malloc: failed on allocation of %i bytes", size);
		if (rover->user)
		{
			if (rover->tag < PU_PURGELEVEL)
			// hit a block that can't be purged, so move base past it
				base = rover = rover->next;
			else
			{
			// free the rover block (adding the size to base)
				base = base->prev;	// the rover can be the base block
				Z_Free ((byte *)rover+sizeof(memblock_t));
				base = base->next;
				rover = base->next;
			}
		}
		else
			rover = rover->next;
	} while (base->user || base->size < size);
	
//
// found a block big enough
//
	extra = base->size - size;
	if (extra >  MINFRAGMENT)
	{	// there will be a free fragment after the allocated block
		new = (memblock_t *) ((byte *)base + size );
		new->size = extra;
		new->user = NULL;		// free block
		new->tag = 0;
		new->prev = base;
		new->next = base->next;
		new->next->prev = new;
		base->next = new;
		base->size = size;
	}
	
	if (user)
	{
		base->user = user;			// mark as an in use block
		*(void **)user = (void *) ((byte *)base + sizeof(memblock_t));
	}
	else
	{
		if (tag >= PU_PURGELEVEL)
			I_Error ("Z_Malloc: an owner is required for purgable blocks");
		base->user = (void *)2;		// mark as in use, but unowned	
	}
	base->tag = tag;
	
	mainzone->rover = base->next;	// next allocation will start looking here
	
	base->id = ZONEID;
	return (void *) ((byte *)base + sizeof(memblock_t));
}


/*
========================
=
= Z_FreeTags
=
========================
*/

void Z_FreeTags (int32_t lowtag, int32_t hightag)
{
	memblock_t	*block, *next;
	
	for (block = mainzone->blocklist.next ; block != &mainzone->blocklist 
	; block = next)
	{
		next = block->next;		// get link before freeing
		if (!block->user)
			continue;			// free block
		if (block->tag >= lowtag && block->tag <= hightag)
			Z_Free ( (byte *)block+sizeof(memblock_t));
	}
}

/*
========================
=
= Z_CheckHeap
=
========================
*/

void Z_CheckHeap (void)
{
	memblock_t	*block;
	
	for (block = mainzone->blocklist.next ; ; block = block->next)
	{
		if (block->next == &mainzone->blocklist)
			break;			// all blocks have been hit	
		if ( (byte *)block + block->size != (byte *)block->next)
			I_Error ("Z_CheckHeap: block size does not touch the next block\n");
		if ( block->next->prev != block)
			I_Error ("Z_CheckHeap: next block doesn't have proper back link\n");
		if (!block->user && !block->next->user)
			I_Error ("Z_CheckHeap: two consecutive free blocks\n");
	}
}


/*
========================
=
= Z_ChangeTag
=
========================
*/

void Z_ChangeTag2 (void *ptr, int32_t tag)
{
	memblock_t	*block;
	
	block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID)
		I_Error ("Z_ChangeTag: freed a pointer without ZONEID");
	if (tag >= PU_PURGELEVEL && (uint32_t)block->user < 0x100)
		I_Error ("Z_ChangeTag: an owner is required for purgable blocks");
	block->tag = tag;
}
