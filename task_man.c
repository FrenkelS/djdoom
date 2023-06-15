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

#define TRUE  ( 1 == 1 )
#define FALSE ( !TRUE )

#define NOINTS

#include <stdlib.h>
#include <dos.h>
#include <conio.h>
#include <string.h>
#include "interrup.h"
#include "task_man.h"

#define FreeMem( ptr )   USRHOOKS_FreeMem( ( ptr ) )

typedef struct
   {
   task *start;
   task *end;
   } tasklist;


#define NewNode(type)  ((type*)SafeMalloc(sizeof(type)))


#define LL_CreateNewLinkedList(rootnode,type,next,prev) 			\
   { 																				\
   (rootnode) = NewNode(type);                            		\
   (rootnode)->prev = (rootnode);                         		\
   (rootnode)->next = (rootnode);                         		\
   }



#define LL_AddNode(rootnode, newnode, next, prev) 			\
   {                                              			\
   (newnode)->next = (rootnode);                  			\
   (newnode)->prev = (rootnode)->prev;                	\
   (rootnode)->prev->next = (newnode);                	\
   (rootnode)->prev = (newnode);                      	\
   }

#define LL_TransferList(oldroot,newroot,next,prev)  \
   {                                                \
   if ((oldroot)->prev != (oldroot))                    \
      {                                             \
      (oldroot)->prev->next = (newroot);                \
      (oldroot)->next->prev = (newroot)->prev;          \
      (newroot)->prev->next = (oldroot)->next;          \
      (newroot)->prev = (oldroot)->prev;                \
      (oldroot)->next = (oldroot);                      \
      (oldroot)->prev = (oldroot);                      \
      }                                             \
   }

#define LL_ReverseList(root,type,next,prev)              \
   {                                                     \
   type *newend,*trav,*tprev;                            \
                                                         \
   newend = (root)->next;                                  \
   for(trav = (root)->prev; trav != newend; trav = tprev)  \
      {                                                  \
      tprev = trav->prev;                                \
      LL_MoveNode(trav,newend,next,prev);                \
      }                                                  \
   }


#define LL_RemoveNode(node,next,prev) \
   {                                  \
   (node)->prev->next = (node)->next;     \
   (node)->next->prev = (node)->prev;     \
   (node)->next = (node);                 \
   (node)->prev = (node);                 \
   }


#define LL_SortedInsertion(rootnode,insertnode,next,prev,type,sortparm) \
   {                                                                    \
   type *hoya;                                                          \
                                                                        \
   hoya = (rootnode)->next;                                               \
   while((hoya != (rootnode)) && ((insertnode)->sortparm > hoya->sortparm)) \
      {                                                                 \
      hoya = hoya->next;                                                \
      }                                                                 \
   LL_AddNode(hoya,(insertnode),next,prev);                               \
   }

#define LL_MoveNode(node,newroot,next,prev) \
   {                                        \
   LL_RemoveNode((node),next,prev);           \
   LL_AddNode((newroot),(node),next,prev);      \
   }

#define LL_ListEmpty(list,next,prev) \
   (                                 \
   ((list)->next == (list)) &&       \
   ((list)->prev == (list))          \
   )

#define LL_Free(list)   SafeFree(list)
#define LL_Reset(list,next,prev)    (list)->next = (list)->prev = (list)
#define LL_New      LL_CreateNewLinkedList
#define LL_Remove   LL_RemoveNode
#define LL_Add      LL_AddNode
#define LL_Empty    LL_ListEmpty
#define LL_Move     LL_MoveNode


/*---------------------------------------------------------------------
   Global variables
---------------------------------------------------------------------*/

static task HeadTask;
static task *TaskList = &HeadTask;

static void ( __interrupt __far *OldInt8 )( void );

static volatile int32_t TaskServiceRate  = 0x10000L;
static volatile int32_t TaskServiceCount = 0;

#ifndef NOINTS
static volatile int  TS_TimesInInterrupt;
#endif

static char TS_Installed = FALSE;

static volatile int TS_InInterrupt = FALSE;

/*---------------------------------------------------------------------
   Function prototypes
---------------------------------------------------------------------*/

static void TS_FreeTaskList( void );
static void TS_SetClockSpeed( long speed );
static long TS_SetTimer( long TickBase );
static void TS_SetTimerToMaxTaskRate( void );
static void __interrupt __far TS_ServiceSchedule( void );
static void __interrupt __far TS_ServiceScheduleIntEnabled( void );
static void TS_AddTask( task *ptr );
static int  TS_Startup( void );
static void RestoreRealTimeClock( void );


/*---------------------------------------------------------------------
   Function: TS_FreeTaskList

   Terminates all tasks and releases any memory used for control
   structures.
---------------------------------------------------------------------*/

static void TS_FreeTaskList
   (
   void
   )

   {
   task *node;
   task *next;
   unsigned flags;

   flags = DisableInterrupts();

   node = TaskList->next;
   while( node != TaskList )
      {
      next = node->next;
      FreeMem( node );
      node = next;
      }

   TaskList->next = TaskList;
   TaskList->prev = TaskList;

   RestoreInterrupts( flags );
   }


/*---------------------------------------------------------------------
   Function: TS_SetClockSpeed

   Sets the rate of the 8253 timer.
---------------------------------------------------------------------*/

static void TS_SetClockSpeed
   (
   long speed
   )

   {
   unsigned flags;

   flags = DisableInterrupts();

   if ( ( speed > 0 ) && ( speed < 0x10000L ) )
      {
      TaskServiceRate = speed;
      }
   else
      {
      TaskServiceRate = 0x10000L;
      }

   outp( 0x43, 0x36 );
   outp( 0x40, TaskServiceRate );
   outp( 0x40, TaskServiceRate >> 8 );

   RestoreInterrupts( flags );
   }


/*---------------------------------------------------------------------
   Function: TS_SetTimer

   Calculates the rate at which a task will occur and sets the clock
   speed if necessary.
---------------------------------------------------------------------*/

static long TS_SetTimer
   (
   long TickBase
   )

   {
   long speed;

   speed = 1192030L / TickBase;
   if ( speed < TaskServiceRate )
      {
      TS_SetClockSpeed( speed );
      }

   return( speed );
   }


/*---------------------------------------------------------------------
   Function: TS_SetTimerToMaxTaskRate

   Finds the fastest running task and sets the clock to operate at
   that speed.
---------------------------------------------------------------------*/

static void TS_SetTimerToMaxTaskRate
   (
   void
   )

   {
   task     *ptr;
   long      MaxServiceRate;
   unsigned  flags;

   flags = DisableInterrupts();

   MaxServiceRate = 0x10000L;

   ptr = TaskList->next;
   while( ptr != TaskList )
      {
      if ( ptr->rate < MaxServiceRate )
         {
         MaxServiceRate = ptr->rate;
         }

      ptr = ptr->next;
      }

   if ( TaskServiceRate != MaxServiceRate )
      {
      TS_SetClockSpeed( MaxServiceRate );
      }

   RestoreInterrupts( flags );
   }


#ifdef NOINTS
/*---------------------------------------------------------------------
   Function: TS_ServiceSchedule

   Interrupt service routine
---------------------------------------------------------------------*/

static void __interrupt __far TS_ServiceSchedule
   (
   void
   )

   {
   task *ptr;
   task *next;


   TS_InInterrupt = TRUE;

   ptr = TaskList->next;
   while( ptr != TaskList )
      {
      next = ptr->next;

      if ( ptr->active )
         {
         ptr->count += TaskServiceRate;

         while( ptr->count >= ptr->rate )
            {
            ptr->count -= ptr->rate;
            ptr->TaskService( ptr );
            }
         }
      ptr = next;
      }

   TaskServiceCount += TaskServiceRate;
   if ( TaskServiceCount > 0xffffL )
      {
      TaskServiceCount &= 0xffff;
      _chain_intr( OldInt8 );
      }

   outp( 0x20,0x20 );

   TS_InInterrupt = FALSE;
   }

#else

/*---------------------------------------------------------------------
   Function: TS_ServiceScheduleIntEnabled

   Interrupt service routine with interrupts enabled.
---------------------------------------------------------------------*/

static void __interrupt __far TS_ServiceScheduleIntEnabled
   (
   void
   )

   {
   task *ptr;
   task *next;

   TS_TimesInInterrupt++;
   TaskServiceCount += TaskServiceRate;
   if ( TaskServiceCount > 0xffffL )
      {
      TaskServiceCount &= 0xffff;
      _chain_intr( OldInt8 );
      }

   outp( 0x20,0x20 );

   if ( TS_InInterrupt )
      {
      return;
      }

   TS_InInterrupt = TRUE;
   _enable();

   while( TS_TimesInInterrupt )
      {
      ptr = TaskList->next ;
      while( ptr != TaskList )
         {
         next = ptr->next;

         if ( ptr->active )
            {
            ptr->count += TaskServiceRate;
            if ( ptr->count >= ptr->rate )
               {
               ptr->count -= ptr->rate;
               ptr->TaskService( ptr );
               }
            }
         ptr = next;
         }
      TS_TimesInInterrupt--;
      }

   _disable();

   TS_InInterrupt = FALSE;
   }
#endif


/*---------------------------------------------------------------------
   Function: TS_Startup

   Sets up the task service routine.
---------------------------------------------------------------------*/

static int TS_Startup
   (
   void
   )

   {
   if ( !TS_Installed )
      {

//static const task *TaskList = &HeadTask;
      TaskList->next = TaskList;
      TaskList->prev = TaskList;

      TaskServiceRate  = 0x10000L;
      TaskServiceCount = 0;

#ifndef NOINTS
      TS_TimesInInterrupt = 0;
#endif

      OldInt8 = _dos_getvect( 0x08 );
      #ifdef NOINTS
         _dos_setvect( 0x08, TS_ServiceSchedule );
      #else
         _dos_setvect( 0x08, TS_ServiceScheduleIntEnabled );
      #endif

      TS_Installed = TRUE;
      }

   return( TASK_Ok );
   }


/*---------------------------------------------------------------------
   Function: TS_Shutdown

   Ends processing of all tasks.
---------------------------------------------------------------------*/

void TS_Shutdown
   (
   void
   )

   {
   if ( TS_Installed )
      {
      TS_FreeTaskList();

      TS_SetClockSpeed( 0 );

      _dos_setvect( 0x08, OldInt8 );

      TS_Installed = FALSE;
      }
   }


/*---------------------------------------------------------------------
   Error definitions
---------------------------------------------------------------------*/

enum USRHOOKS_Errors
   {
   USRHOOKS_Warning = -2,
   USRHOOKS_Error   = -1,
   USRHOOKS_Ok      = 0
   };


/*---------------------------------------------------------------------
   Function: USRHOOKS_GetMem

   Allocates the requested amount of memory and returns a pointer to
   its location, or NULL if an error occurs.  NOTE: pointer is assumed
   to be dword aligned.
---------------------------------------------------------------------*/

static int USRHOOKS_GetMem
   (
   void **ptr,
   unsigned long size
   )

   {
   void *memory;

   memory = malloc( size );
   if ( memory == NULL )
      {
      return( USRHOOKS_Error );
      }

   *ptr = memory;

   return( USRHOOKS_Ok );
   }


/*---------------------------------------------------------------------
   Function: USRHOOKS_FreeMem

   Deallocates the memory associated with the specified pointer.
---------------------------------------------------------------------*/

static int USRHOOKS_FreeMem
   (
   void *ptr
   )

   {
   if ( ptr == NULL )
      {
      return( USRHOOKS_Error );
      }

   free( ptr );

   return( USRHOOKS_Ok );
   }


/*---------------------------------------------------------------------
   Function: TS_ScheduleTask

   Schedules a new task for processing.
---------------------------------------------------------------------*/

task *TS_ScheduleTask
   (
   void  ( *Function )( task * ),
   int   rate,
   int   priority,
   void *data
   )

   {
   task *ptr;

   int   status;

   ptr = NULL;

   status = USRHOOKS_GetMem((void **) &ptr, sizeof( task ) );
   if ( status == USRHOOKS_Ok )
      {
      if ( !TS_Installed )
         {
         status = TS_Startup();
         if ( status != TASK_Ok )
            {
            FreeMem( ptr );
            return( NULL );
            }
         }

      ptr->TaskService = Function;
      ptr->taskId = data;
      ptr->rate = TS_SetTimer( rate );
      ptr->count = 0;
      ptr->priority = priority;
      ptr->active = FALSE;

      TS_AddTask( ptr );
      }

   return( ptr );
   }


/*---------------------------------------------------------------------
   Function: TS_AddTask

   Adds a new task to our list of tasks.
---------------------------------------------------------------------*/

static void TS_AddTask
   (
   task *node
   )

   {
   LL_SortedInsertion( TaskList, node, next, prev, task, priority );
   }


/*---------------------------------------------------------------------
   Function: TS_Terminate

   Ends processing of a specific task.
---------------------------------------------------------------------*/

int TS_Terminate
   (
   task *NodeToRemove
   )

   {
   task *ptr;
   task *next;
   unsigned flags;

   flags = DisableInterrupts();

   ptr = TaskList->next;
   while( ptr != TaskList )
      {
      next = ptr->next;

      if ( ptr == NodeToRemove )
         {
         LL_RemoveNode( NodeToRemove, next, prev );
         NodeToRemove->next = NULL;
         NodeToRemove->prev = NULL;
         FreeMem( NodeToRemove );

         TS_SetTimerToMaxTaskRate();

         RestoreInterrupts( flags );

         return( TASK_Ok );
         }

      ptr = next;
      }

   RestoreInterrupts( flags );

   return( TASK_Warning );
   }


/*---------------------------------------------------------------------
   Function: TS_Dispatch

   Begins processing of all inactive tasks.
---------------------------------------------------------------------*/

void TS_Dispatch
   (
   void
   )

   {
   task *ptr;
   unsigned flags;

   flags = DisableInterrupts();

   ptr = TaskList->next;
   while( ptr != TaskList )
      {
      ptr->active = TRUE;
      ptr = ptr->next;
      }

   RestoreInterrupts( flags );
   }


/*---------------------------------------------------------------------
   Function: TS_SetTaskRate

   Sets the rate at which the specified task is serviced.
---------------------------------------------------------------------*/

void TS_SetTaskRate
   (
   task *Task,
   int rate
   )

   {
   unsigned flags;

   flags = DisableInterrupts();

   Task->rate = TS_SetTimer( rate );
   TS_SetTimerToMaxTaskRate();

   RestoreInterrupts( flags );
   }
