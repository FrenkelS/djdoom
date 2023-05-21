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

#ifndef _TSMAPI_H_
#define _TSMAPI_H_

int32_t TSM_Install(uint32_t sndTicrate);
int32_t TSM_NewService(int32_t (*timerISR)(void), int32_t frequency, int32_t priority, int32_t pause);
void TSM_DelService(int32_t tsm_ID);
void TSM_Remove(void);

#endif
