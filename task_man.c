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

#if defined __DJGPP__
static _go32_dpmi_seginfo OldInt8, NewInt8;
#elif defined __CCDL__
static uint16_t OldInt8selector;
static uint32_t OldInt8offset;
#elif defined __WATCOMC__
static void (_interrupt _far *OldInt8)(void);
#endif

static volatile int32_t TaskServiceRate  = 0x10000L;
static volatile int32_t TaskServiceCount = 0;

static boolean TS_Installed = false;


#include "interrup.h"


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
	outp(0x40, LOBYTE(TaskServiceRate));
	outp(0x40, HIBYTE(TaskServiceRate));

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

#if defined __DJGPP__
static void TS_ServiceSchedule (void)
#elif defined __CCDL__ || defined __WATCOMC__
static void _interrupt _far TS_ServiceSchedule (void)
#elif defined __DMC__
static int TS_ServiceSchedule (struct INT_DATA *pd)
#endif
{
	task *ptr;
	task *next;

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
#if defined __DJGPP__
		asm
		(
			"cli \n"
			"pushfl \n"
			"lcall *%0"
			:
			: "m" (OldInt8.pm_offset)
		);
#elif defined __WATCOMC__
		_chain_intr(OldInt8);
#elif defined __CCDL__
		//TODO call OldInt8() instead of acknowledging the interrupt
		outp(0x20, 0x20);
#elif defined __DMC__
		return 0;
#endif
	} else {
		outp(0x20, 0x20);
#if defined __DMC__
		return 1;
#endif
	}
}


/*---------------------------------------------------------------------
   Function: TS_Startup

   Sets up the task service routine.
---------------------------------------------------------------------*/

#define TIMERINT 8

static void TS_Startup(void)
{
	if (!TS_Installed)
	{
		TaskList->next = TaskList;
		TaskList->prev = TaskList;

		TaskServiceRate  = 0x10000L;
		TaskServiceCount = 0;

#if defined __DJGPP__
		_go32_dpmi_get_protected_mode_interrupt_vector(TIMERINT, &OldInt8);

		NewInt8.pm_selector = _go32_my_cs(); 
		NewInt8.pm_offset = (int32_t)TS_ServiceSchedule;
		_go32_dpmi_allocate_iret_wrapper(&NewInt8);
		_go32_dpmi_set_protected_mode_interrupt_vector(TIMERINT, &NewInt8);
#elif defined __DMC__
		int_intercept(TIMERINT, TS_ServiceSchedule, 0);
#elif defined __CCDL__
		{
			struct SREGS	segregs;

			_segread(&segregs);
			dpmi_get_protected_interrupt(&OldInt8selector, &OldInt8offset, TIMERINT);
			dpmi_set_protected_interrupt(TIMERINT, segregs.cs, (uint32_t)TS_ServiceSchedule);
		}
#elif defined __WATCOMC__
		OldInt8 = _dos_getvect(TIMERINT);
		_dos_setvect(TIMERINT, TS_ServiceSchedule);
#endif

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

#if defined __DJGPP__
		_go32_dpmi_set_protected_mode_interrupt_vector(TIMERINT, &OldInt8);
		_go32_dpmi_free_iret_wrapper(&NewInt8);
#elif defined __DMC__
		int_restore(TIMERINT);
#elif defined __CCDL__
		dpmi_set_protected_interrupt(TIMERINT, OldInt8selector, OldInt8offset);
#elif defined __WATCOMC__
		_dos_setvect(TIMERINT, OldInt8);
#endif

		TS_Installed = false;
	}
}


/*---------------------------------------------------------------------
   Function: TS_AddTask

   Adds a new task to our list of tasks.
---------------------------------------------------------------------*/

static void TS_AddTask(task *node)
{
	task *ptr;

	ptr = TaskList->next;
	while ((ptr != TaskList) && (node->priority > ptr->priority))
		ptr = ptr->next;

	node->next = ptr;
	node->prev = ptr->prev;
	ptr->prev->next = node;
	ptr->prev = node;
}


/*---------------------------------------------------------------------
   Function: TS_ScheduleTask

   Schedules a new task for processing.
---------------------------------------------------------------------*/

task *TS_ScheduleTask(void (*Function)(task *), int32_t rate, int32_t priority, int32_t taskId)
{
	task *ptr;

	ptr = malloc(sizeof(task));
	if (ptr != NULL)
	{
		if (!TS_Installed)
			TS_Startup();

		ptr->TaskService = Function;
		ptr->taskId = taskId;
		ptr->rate = TS_SetTimer(rate);
		ptr->count = 0;
		ptr->priority = priority;
		ptr->active = false;

		TS_AddTask(ptr);
	}

	return ptr;
}


/*---------------------------------------------------------------------
   Function: TS_Terminate

   Ends processing of a specific task.
---------------------------------------------------------------------*/

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
			NodeToRemove->prev->next = NodeToRemove->next;
			NodeToRemove->next->prev = NodeToRemove->prev;
			NodeToRemove->next = NULL;
			NodeToRemove->prev = NULL;
			free(NodeToRemove);

			TS_SetTimerToMaxTaskRate();

			break;
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


/*---------------------------------------------------------------------
   Function: TS_SetTaskRate

   Sets the rate at which the specified task is serviced.
---------------------------------------------------------------------*/

void TS_SetTaskRate(task *Task, int32_t rate)
{
	uint32_t flags = DisableInterrupts();

	Task->rate = TS_SetTimer(rate);
	TS_SetTimerToMaxTaskRate();

	RestoreInterrupts(flags);
}
