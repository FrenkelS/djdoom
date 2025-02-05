//
//
// Copyright (C) 2023-2025 Frenkel Smeijers
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

#include "id_heads.h"
#include "a_taskmn.h"
#include "a_tsmapi.h"

static task *t;
static void (*callback)(void);


void TSM_Install(uint32_t rate)
{
	UNUSED(rate);
}

static void tsm_funch(task *t)
{
	UNUSED(t);
	callback();
}

int32_t TSM_NewService(void(*function)(void), int32_t rate, int32_t priority, int32_t pause)
{
	UNUSED(pause);

	callback = function;
	t = TS_ScheduleTask(tsm_funch, rate, priority);
	TS_Dispatch();
	return 0;
}

void TSM_DelService(int32_t taskId)
{
	UNUSED(taskId);
	TS_Terminate(t);
	t = NULL;
}

void TSM_Remove(void)
{
	TS_Shutdown();
}
