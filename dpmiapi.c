//
// Copyright (C) 1993-1996 Id Software, Inc.
// Copyright (C) 1993-2008 Raven Software
// Copyright (C) 2015-2021 Nuke.YKT
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
#include <dos.h>
#include "dpmiapi.h"

int DPMI_LockMemory(void *address, unsigned length);
int DPMI_UnlockMemory(void *address, unsigned length);

int _dpmi_lockregion(void *address, unsigned length)
{
	return DPMI_LockMemory(address, length);
}

int _dpmi_unlockregion(void *address, unsigned length)
{
	return DPMI_UnlockMemory(address, length);
}
