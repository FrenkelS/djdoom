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

#include <assert.h>

#include <cstdlib>

#include <algorithm>
#include <unordered_map>
#include <memory>
#include <vector>

static std::unordered_map<int32_t, std::vector<void*>> memory_map;
static std::unordered_map<void*, std::pair<void**, uint32_t>> user_map;

extern "C" {

void I_Error(const char* error, ...);

void Z_Init (void) {}


/*
========================
=
= Z_Free
=
========================
*/

void Z_Free (void *ptr) {
	if (!ptr) return;
	auto user = user_map.find(ptr);
	if (user == user_map.end()) I_Error("Z_Free: pointer not found in user_map");
	if (user->second.first) {
		*user->second.first = nullptr; // clear the user's mark
	}
	auto tag = user->second.second;
	user_map.erase(user);
	auto it = memory_map.find(tag);
	if (it == memory_map.end()) {
		I_Error("Z_Free: pointer not found in memory_map");
	}
	auto& ptrs = it->second;
	auto ptr_it = std::find(ptrs.begin(), ptrs.end(), ptr);
	if (ptr_it == ptrs.end()) {
		I_Error("Z_Free: pointer not found in memory_map");
	}
	ptrs.erase(ptr_it);
	std::free(ptr);
}


/*
========================
=
= Z_Malloc
=
= You can pass a NULL user if the tag is < PU_PURGELEVEL
========================
*/

void *Z_Malloc (int32_t size, int32_t tag, void **user)
{
	auto ptr = std::malloc(size);

	memory_map[tag].push_back(ptr);
	user_map[ptr] = {user, tag};
	if (user) {
		*user = ptr;
	}
	return ptr;
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
	for (auto it = memory_map.begin(); it != memory_map.end(); ++it) {
		if (it->first >= lowtag && it->first <= hightag) {
			auto copied = it->second;
			for (auto ptr : copied) {
				Z_Free(ptr);
			}
		}
	}
}

/*
========================
=
= Z_CheckHeap
=
========================
*/

void Z_CheckHeap (void) {}


/*
========================
=
= Z_ChangeTag
=
========================
*/

#define	PU_PURGELEVEL	100

void Z_ChangeTag2 (void *ptr, int32_t tag, const char* file, int line)
{
	auto user = user_map.find(ptr);
	if (user == user_map.end()) {
		I_Error("Z_ChangeTag: pointer not found in user_map");
	}
	if (tag >= PU_PURGELEVEL && user->second.first == nullptr) {
		I_Error("Z_ChangeTag: an owner is required for purgable blocks");
	}
	user->second.second = tag;
}

}  // extern "C"