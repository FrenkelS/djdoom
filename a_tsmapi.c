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

#include "doomdef.h"
#include "a_taskmn.h"
#include "a_tsmapi.h"

typedef struct {
	task *t;
	void (*callback)(void);
} task_t;

#define MAX_TASKS 8

static task_t tasks[MAX_TASKS];

void TSM_Install(uint32_t rate)
{
	UNUSED(rate);
	memset(tasks, 0, sizeof(tasks));
}

static void tsm_funch(task *t)
{
	int32_t taskId = t->taskId;
	tasks[taskId].callback();
}

int32_t TSM_NewService(void(*function)(void), int32_t rate, int32_t priority, int32_t pause)
{
	int32_t taskId;

	UNUSED(pause);

	for (taskId = 0; taskId < MAX_TASKS; taskId++)
	{
		if (tasks[taskId].t == NULL)
			break;
	}

	if (taskId == MAX_TASKS)
		return -1;

	tasks[taskId].callback = function;
	tasks[taskId].t = TS_ScheduleTask(tsm_funch, rate, priority, taskId);
	TS_Dispatch();
	return taskId;
}

void TSM_DelService(int32_t taskId)
{
	if (taskId >= 0)
	{
		TS_Terminate(tasks[taskId].t);
		tasks[taskId].t = NULL;
	}
}

void TSM_Remove(void)
{
	TS_Shutdown();
}
