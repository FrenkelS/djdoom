/*
Copyright (C) 1994-1995 Apogee Software, Ltd.
Copyright (C) 2023 Frenkel Smeijers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/**********************************************************************
   module: TASK_MAN.C

   author: James R. Dose
   date:   July 25, 1994

   Low level timer task scheduler.

   (c) Copyright 1994 James R. Dose.  All Rights Reserved.
**********************************************************************/

#include <conio.h>
#include <dos.h>
#include <stdint.h>
#include <stdlib.h>
#include "doomdef.h"
#include "task_man.h"


typedef struct
{
	task *start;
	task *end;
} tasklist;


/*---------------------------------------------------------------------
   Global variables
---------------------------------------------------------------------*/

static task HeadTask;
static task *TaskList = &HeadTask;

static void (__interrupt __far *OldInt8)(void);

static volatile long TaskServiceRate  = 0x10000L;
static volatile long TaskServiceCount = 0;

static boolean TS_Installed = false;

static volatile boolean TS_InInterrupt = false;


/*---------------------------------------------------------------------
   Function prototypes
---------------------------------------------------------------------*/

static uint32_t DisableInterrupts(void);
#pragma aux DisableInterrupts =	\
   "pushfd",					\
   "pop eax",					\
   "cli";


static void RestoreInterrupts(uint32_t flags);
#pragma aux RestoreInterrupts =	\
   "push eax",					\
   "popfd"						\
   parm [eax];


/*---------------------------------------------------------------------
   Function: TS_FreeTaskList

   Terminates all tasks and releases any memory used for control
   structures.
---------------------------------------------------------------------*/

static void TS_FreeTaskList(void)
{
	task *node;
	task *next;

	uint32_t flags = DisableInterrupts();

	node = TaskList->next;
	while (node != TaskList)
	{
		next = node->next;
		free(node);
		node = next;
	}

	TaskList->next = TaskList;
	TaskList->prev = TaskList;

	RestoreInterrupts(flags);
}


/*---------------------------------------------------------------------
   Function: TS_SetClockSpeed

   Sets the rate of the 8253 timer.
---------------------------------------------------------------------*/

static void TS_SetClockSpeed(int32_t speed)
{
	uint32_t flags = DisableInterrupts();

	if ((0 < speed) && (speed < 0x10000L))
		TaskServiceRate = speed;
	else
		TaskServiceRate = 0x10000L;

	outp(0x43, 0x36);
	outp(0x40, TaskServiceRate);
	outp(0x40, TaskServiceRate >> 8);

	RestoreInterrupts(flags);
}


/*---------------------------------------------------------------------
   Function: TS_SetTimer

   Calculates the rate at which a task will occur and sets the clock
   speed if necessary.
---------------------------------------------------------------------*/

static int32_t TS_SetTimer(int32_t TickBase)
{
	int32_t speed = 1193182L / TickBase;
	if (speed < TaskServiceRate)
		TS_SetClockSpeed(speed);

	return speed;
}


/*---------------------------------------------------------------------
   Function: TS_SetTimerToMaxTaskRate

   Finds the fastest running task and sets the clock to operate at
   that speed.
---------------------------------------------------------------------*/

static void TS_SetTimerToMaxTaskRate(void)
{
	task     *ptr;
	int32_t  MaxServiceRate;

	uint32_t flags = DisableInterrupts();

	MaxServiceRate = 0x10000L;

	ptr = TaskList->next;
	while (ptr != TaskList)
	{
		if (ptr->rate < MaxServiceRate)
			MaxServiceRate = ptr->rate;

		ptr = ptr->next;
	}

	if (TaskServiceRate != MaxServiceRate)
		TS_SetClockSpeed(MaxServiceRate);

	RestoreInterrupts(flags);
}


/*---------------------------------------------------------------------
   Function: TS_ServiceSchedule

   Interrupt service routine
---------------------------------------------------------------------*/

static void __interrupt __far TS_ServiceSchedule(void)
{
	task *ptr;
	task *next;

	TS_InInterrupt = true;

	ptr = TaskList->next;
	while (ptr != TaskList)
	{
		next = ptr->next;

		if (ptr->active)
		{
			ptr->count += TaskServiceRate;

			while (ptr->count >= ptr->rate)
			{
				ptr->count -= ptr->rate;
				ptr->TaskService(ptr);
			}
		}
		ptr = next;
	}

	TaskServiceCount += TaskServiceRate;
	if (TaskServiceCount > 0xffffL)
	{
		TaskServiceCount &= 0xffff;
		_chain_intr(OldInt8);
	}

	outp(0x20, 0x20);

	TS_InInterrupt = false;
}


/*---------------------------------------------------------------------
   Function: TS_Startup

   Sets up the task service routine.
---------------------------------------------------------------------*/

static void TS_Startup(void)
{
	if (!TS_Installed)
	{
		TaskList->next = TaskList;
		TaskList->prev = TaskList;

		TaskServiceRate  = 0x10000L;
		TaskServiceCount = 0;

		OldInt8 = _dos_getvect(0x08);
		_dos_setvect(0x08, TS_ServiceSchedule);

		TS_Installed = true;
	}
}


/*---------------------------------------------------------------------
   Function: TS_Shutdown

   Ends processing of all tasks.
---------------------------------------------------------------------*/

void TS_Shutdown(void)
{
	if (TS_Installed)
	{
		TS_FreeTaskList();

		TS_SetClockSpeed(0);

		_dos_setvect(0x08, OldInt8);

		TS_Installed = false;
	}
}


/*---------------------------------------------------------------------
   Function: USRHOOKS_GetMem

   Allocates the requested amount of memory and returns a pointer to
   its location, or NULL if an error occurs.  NOTE: pointer is assumed
   to be dword aligned.
---------------------------------------------------------------------*/

enum USRHOOKS_Errors
{
	USRHOOKS_Error	= -1,
	USRHOOKS_Ok		= 0
};

static int32_t USRHOOKS_GetMem(void **ptr, uint32_t size)
{
	void *memory;

	memory = malloc(size);
	if (memory == NULL)
		return USRHOOKS_Error;
	else
	{
		*ptr = memory;
		return USRHOOKS_Ok;
	}
}


/*---------------------------------------------------------------------
   Function: TS_AddTask

   Adds a new task to our list of tasks.
---------------------------------------------------------------------*/

#define LL_AddNode(rootnode, newnode, next, prev)	\
	{												\
		(newnode)->next = (rootnode);				\
		(newnode)->prev = (rootnode)->prev;			\
		(rootnode)->prev->next = (newnode);			\
		(rootnode)->prev = (newnode);				\
	}

#define LL_SortedInsertion(rootnode,insertnode,next,prev,type,sortparm)				\
	{																				\
		type *hoya;																	\
																					\
		hoya = (rootnode)->next;													\
		while((hoya != (rootnode)) && ((insertnode)->sortparm > hoya->sortparm))	\
			hoya = hoya->next;														\
																					\
		LL_AddNode(hoya,(insertnode),next,prev);									\
   }

static void TS_AddTask(task *node)
{
	LL_SortedInsertion(TaskList, node, next, prev, task, priority);
}


/*---------------------------------------------------------------------
   Function: TS_ScheduleTask

   Schedules a new task for processing.
---------------------------------------------------------------------*/

task *TS_ScheduleTask(void (*Function)(task *), int32_t rate, int32_t priority, void *data)
{
	task *ptr;

	int32_t status;

	ptr = NULL;

	status = USRHOOKS_GetMem((void **) &ptr, sizeof(task));
	if (status == USRHOOKS_Ok)
	{
		if (!TS_Installed)
			TS_Startup();

		ptr->TaskService = Function;
		ptr->data = data;
		ptr->rate = TS_SetTimer(rate);
		ptr->count = 0;
		ptr->priority = priority;
		ptr->active = false;

		TS_AddTask( ptr );
	}

	return ptr;
}


/*---------------------------------------------------------------------
   Function: TS_Terminate

   Ends processing of a specific task.
---------------------------------------------------------------------*/

#define LL_RemoveNode(node,next,prev)		\
	{										\
		(node)->prev->next = (node)->next;	\
		(node)->next->prev = (node)->prev;	\
		(node)->next = (node);				\
		(node)->prev = (node);				\
	}

void TS_Terminate(task *NodeToRemove)
{
	task *ptr;
	task *next;
   
	uint32_t flags = DisableInterrupts();

	ptr = TaskList->next;
	while (ptr != TaskList)
	{
		next = ptr->next;

		if (ptr == NodeToRemove)
		{
			LL_RemoveNode(NodeToRemove, next, prev);
			NodeToRemove->next = NULL;
			NodeToRemove->prev = NULL;
			free(NodeToRemove);

			TS_SetTimerToMaxTaskRate();

			RestoreInterrupts(flags);

			return;
		}

		ptr = next;
	}

	RestoreInterrupts(flags);
}


/*---------------------------------------------------------------------
   Function: TS_Dispatch

   Begins processing of all inactive tasks.
---------------------------------------------------------------------*/

void TS_Dispatch(void)
{
	task *ptr;
   
	uint32_t flags = DisableInterrupts();

	ptr = TaskList->next;
	while (ptr != TaskList)
	{
		ptr->active = true;
		ptr = ptr->next;
	}

	RestoreInterrupts(flags);
}
