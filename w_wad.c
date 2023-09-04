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

// W_wad.c

#include <ctype.h>
#include <io.h>
#include <unistd.h>
#include <fcntl.h>

#include "doomdef.h"


//===============
//   TYPES
//===============


typedef struct
{
	char		identification[4];		// should be IWAD
	int32_t		numlumps;
	int32_t		infotableofs;
} wadinfo_t;


typedef struct
{
	int32_t		filepos;
	int32_t		size;
	char		name[8];
} filelump_t;


//=============
// GLOBALS
//=============

lumpinfo_t	*lumpinfo;		// location of each lump on disk
static int32_t			numlumps;

static void		**lumpcache;


//===================


static void ExtractFileBase (char *path, char *dest)
{
	char	*src;
	int32_t	length;

	src = path + strlen(path) - 1;

//
// back up until a \ or the start
//
	while (src != path && *(src-1) != '\\' && *(src-1) != '/')
		src--;

//
// copy up to eight characters
//
	memset (dest,0,8);
	length = 0;
	while (*src && *src != '.')
	{
		if (++length == 9)
		{
			I_Error ("Filename base of %s >8 chars",path);
			break; // shut up warning
		}
		*dest++ = toupper((int32_t)*src++);
	}
}

/*
============================================================================

						LUMP BASED ROUTINES

============================================================================
*/

/*
====================
=
= W_AddFile
=
= All files are optional, but at least one file must be found
= Files with a .wad extension are wadlink files with multiple lumps
= Other files are single lumps with the base filename for the lump name
=
====================
*/

static int32_t		reloadlump;
static char			*reloadname;

static void W_AddFile (char *filename)
{
	wadinfo_t		header;
	lumpinfo_t		*lump_p;
	uint32_t		i;
	int32_t			handle, length;
	int32_t			startlump;
	filelump_t		*fileinfo, singleinfo;
	int32_t			storehandle;
	
//
// open the file and add to directory
//	
	if (filename[0] == '~')		// handle reload indicator
	{
		filename++;
		reloadname = filename;
		reloadlump = numlumps;
	}
	if ( (handle = open (filename,O_RDONLY | O_BINARY)) == -1)
	{
		printf ("	couldn't open %s\n",filename);
		return;
	}

	printf ("	adding %s\n",filename);
	startlump = numlumps;
	
	if (stricmp (filename+strlen(filename)-3 , "wad" ) )
	{
	// single lump file
		fileinfo = &singleinfo;
		singleinfo.filepos = 0;
		singleinfo.size = LONG(filelength(handle));
		ExtractFileBase (filename, singleinfo.name);
		numlumps++;
	}
	else 
	{
	// WAD file
		read (handle, &header, sizeof(header));
		if (strncmp(header.identification,"IWAD",4))
		{
			if (strncmp(header.identification,"PWAD",4))
				I_Error ("Wad file %s doesn't have IWAD or PWAD id\n"
				,filename);
			modifiedgame = true;
		}
		header.numlumps = LONG(header.numlumps);
		header.infotableofs = LONG(header.infotableofs);
		length = header.numlumps*sizeof(filelump_t);
		fileinfo = alloca (length);
		lseek (handle, header.infotableofs, SEEK_SET);
		read (handle, fileinfo, length);
		numlumps += header.numlumps;
	}

//
// Fill in lumpinfo
//
	lumpinfo = realloc (lumpinfo, numlumps*sizeof(lumpinfo_t));
	if (!lumpinfo)
		I_Error ("Couldn't realloc lumpinfo");
	lump_p = &lumpinfo[startlump];
	storehandle = reloadname ? -1 : handle;
	for (i=startlump ; i<numlumps ; i++,lump_p++, fileinfo++)
	{
		lump_p->handle = storehandle;
		lump_p->position = LONG(fileinfo->filepos);
		lump_p->size = LONG(fileinfo->size);
		strncpy (lump_p->name, fileinfo->name, 8);
	}
	if (reloadname)
		close (handle);
}




/*
====================
=
= W_Reload
=
= Flushes any of the reloadable lumps in memory and reloads the directory
=
====================
*/

void W_Reload (void)
{
	wadinfo_t		header;
	int32_t			lumpcount;
	lumpinfo_t		*lump_p;
	uint32_t		i;
	int32_t			handle;
	int32_t			length;
	filelump_t		*fileinfo;
	
	if (!reloadname)
		return;
		
	if ( (handle = open (reloadname,O_RDONLY | O_BINARY)) == -1)
		I_Error ("W_Reload: couldn't open %s",reloadname);

	read (handle, &header, sizeof(header));
	lumpcount = LONG(header.numlumps);
	header.infotableofs = LONG(header.infotableofs);
	length = lumpcount*sizeof(filelump_t);
	fileinfo = alloca (length);
	lseek (handle, header.infotableofs, SEEK_SET);
	read (handle, fileinfo, length);
    
//
// Fill in lumpinfo
//
	lump_p = &lumpinfo[reloadlump];
	
	for (i=reloadlump ; i<reloadlump+lumpcount ; i++,lump_p++, fileinfo++)
	{
		if (lumpcache[i])
			Z_Free (lumpcache[i]);

		lump_p->position = LONG(fileinfo->filepos);
		lump_p->size = LONG(fileinfo->size);
	}
	
	close (handle);
}



/*
====================
=
= W_InitMultipleFiles
=
= Pass a null terminated list of files to use.
=
= All files are optional, but at least one file must be found
=
= Files with a .wad extension are idlink files with multiple lumps
=
= Other files are single lumps with the base filename for the lump name
=
= Lump names can appear multiple times. The name searcher looks backwards,
= so a later file can override an earlier one.
=
====================
*/

void W_InitMultipleFiles (char **filenames)
{	
	int32_t		size;
	
//
// open all the files, load headers, and count lumps
//
	numlumps = 0;
	lumpinfo = malloc(1);	// will be realloced as lumps are added

	for ( ; *filenames ; filenames++)
		W_AddFile (*filenames);

	if (!numlumps)
		I_Error ("W_InitMultipleFiles: no files found");
		
//
// set up caching
//
	size = numlumps * sizeof(*lumpcache);
	lumpcache = malloc (size);
	if (!lumpcache)
		I_Error ("Couldn't allocate lumpcache");
	memset (lumpcache,0, size);
}



/*
====================
=
= W_CheckNumForName
=
= Returns -1 if name not found
=
====================
*/

int32_t	W_CheckNumForName (char *name)
{
	char	name8[9];
	int64_t		v;
	lumpinfo_t	*lump_p;

// make the name into two integers for easy compares

	strncpy (name8,name,8);
	name8[8] = 0;			// in case the name was a full 8 chars
	strupr (name8);			// case insensitive

	v = *(int64_t *)name8;


// scan backwards so patch lump files take precedence

	lump_p = lumpinfo + numlumps;

	while (lump_p-- != lumpinfo)
		if ( *(int64_t *)lump_p->name == v)
			return lump_p - lumpinfo;


	return -1;
}


/*
====================
=
= W_GetNumForName
=
= Calls W_CheckNumForName, but bombs out if not found
=
====================
*/

int32_t	W_GetNumForName (char *name)
{
	int32_t	i;

	i = W_CheckNumForName (name);
	if (i == -1)
		I_Error ("W_GetNumForName: %s not found!",name);

	return i;
}


/*
====================
=
= W_LumpLength
=
= Returns the buffer size needed to load the given lump
=
====================
*/

int32_t W_LumpLength (int32_t lump)
{
	if (lump >= numlumps)
		I_Error ("W_LumpLength: %i >= numlumps",lump);
	return lumpinfo[lump].size;
}


/*
====================
=
= W_ReadLump
=
= Loads the lump into the given buffer, which must be >= W_LumpLength()
=
====================
*/

void W_ReadLump (int32_t lump, void *dest)
{
	int32_t		c;
	lumpinfo_t	*l;
	int32_t		handle;
	
	if (lump >= numlumps)
		I_Error ("W_ReadLump: %i >= numlumps",lump);
	l = lumpinfo+lump;
	
	I_BeginRead ();
	if (l->handle == -1)
	{	// reloadable file, so use open / read / close
		if ( (handle = open (reloadname,O_RDONLY | O_BINARY)) == -1)
			I_Error ("W_ReadLump: couldn't open %s",reloadname);
	}
	else
		handle = l->handle;
	lseek (handle, l->position, SEEK_SET);
	c = read (handle, dest, l->size);
	if (c < l->size)
		I_Error ("W_ReadLump: only read %i of %i on lump %i",c,l->size,lump);	
	if (l->handle == -1)
		close (handle);
	I_EndRead ();
}



/*
====================
=
= W_CacheLumpNum
=
====================
*/

void	*W_CacheLumpNum (int32_t lump, int32_t tag)
{

	if ((uint32_t)lump >= numlumps)
		I_Error ("W_CacheLumpNum: %i >= numlumps",lump);
		
	if (!lumpcache[lump])
	{	// read the lump in
//printf ("cache miss on lump %i\n",lump);
		Z_Malloc (W_LumpLength (lump), tag, &lumpcache[lump]);
		W_ReadLump (lump, lumpcache[lump]);
	}
	else
	{
//printf ("cache hit on lump %i\n",lump);
		Z_ChangeTag (lumpcache[lump],tag);
	}
	
	return lumpcache[lump];
}


/*
====================
=
= W_CacheLumpName
=
====================
*/

void	*W_CacheLumpName (char *name, int32_t tag)
{
	return W_CacheLumpNum (W_GetNumForName(name), tag);
}
