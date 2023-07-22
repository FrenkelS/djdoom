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

// Header files, typedefs and macros that are used both in Doom and DMX.

#ifndef __ID_HEADS__
#define __ID_HEADS__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"

#ifndef __BYTEBOOL__
#define __BYTEBOOL__
typedef enum {false, true} boolean;
typedef uint8_t byte;
#endif

#define UNUSED(x)	(x = x)	// for pesky compiler / lint warnings

#define LOBYTE(w)	(((uint8_t *)&w)[0])
#define HIBYTE(w)	(((uint8_t *)&w)[1])

#endif
