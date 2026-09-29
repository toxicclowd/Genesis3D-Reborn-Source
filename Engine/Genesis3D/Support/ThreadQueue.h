/****************************************************************************************/
/*  THREADQUEUE.H                                                                       */
/*                                                                                      */
/*  Author:  Eli Boling                                                                 */
/*  Description:                                                                        */
/*                                                                                      */
/*  The contents of this file are subject to the Jet3D Public License                   */
/*  Version 1.02 (the "License"); you may not use this file except in                   */
/*  compliance with the License. You may obtain a copy of the License at                */
/*  http://www.jet3d.com                                                                */
/*                                                                                      */
/*  Software distributed under the License is distributed on an "AS IS"                 */
/*  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See                */
/*  the License for the specific language governing rights and limitations              */
/*  under the License.                                                                  */
/*                                                                                      */
/*  The Original Code is Jet3D, released December 12, 1999.                             */
/*  Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved           */
/*                                                                                      */
/****************************************************************************************/
#ifndef	THREADQUEUE_H
#define THREADQUEUE_H

#ifdef	__cplusplus
extern "C" {
#endif

#include	"basetype.h"
#include	"ErrorLog.h"

typedef	enum
{
	GR_THREADQUEUE_STATUS_WAITINGFORTHREAD,
	GR_THREADQUEUE_STATUS_WAITINGTOBEGIN,
	GR_THREADQUEUE_STATUS_RUNNING,
	GR_THREADQUEUE_STATUS_COMPLETED,
}	grThreadQueue_JobStatus;
		// these are gauranteed to be in order of execution :
		//  if you want to know if a job is not running yet, you can do
		//		(Status < RUNNING) ?

typedef	struct grThreadQueue_Job		grThreadQueue_Job;
typedef struct grThreadQueue_Semaphore	grThreadQueue_Semaphore;

typedef	void (*grThreadQueue_JobFunction)(grThreadQueue_Job *, void *);

typedef	enum
{
	GR_THREADQUEUE_PRIORITY_HIGH,
	GR_THREADQUEUE_PRIORITY_LOW,
}	grThreadQueue_Priority;

GRAPI	grThreadQueue_Job *	GRCC grThreadQueue_JobCreate(
	grThreadQueue_JobFunction		Function,
	void *		Context,
	grErrorLog *ErrorLog,
	uint32		StackLimit); // <> remove the StackLimit
				// note that when you call this, the job does not actually
				// start until someone calls a PollJos.

GRAPI	void GRCC grThreadQueue_JobCreateRef(grThreadQueue_Job *Job);
GRAPI	void GRCC grThreadQueue_JobDestroy(grThreadQueue_Job **Job);

GRAPI	grThreadQueue_JobStatus	GRCC grThreadQueue_JobGetStatus(const grThreadQueue_Job *Job);

GRAPI	grBoolean GRCC grThreadQueue_JobSetPriority(
	grThreadQueue_Job *		Job,
	grThreadQueue_Priority	Priority);
				// will return false if the priority could not be changed 
				//	(eg. the thread was already running)
				// (not a fatal error)

GRAPI	grThreadQueue_Priority GRCC grThreadQueue_JobGetPriority(grThreadQueue_Job * Job);
				// this does *not* return the result of SetPriority , but instead it
				//	returns the actual realized priority due to location in the queue

GRAPI	grBoolean	GRCC grThreadQueue_SetThreadLimit(int MaxThreads);
GRAPI	int			GRCC grThreadQueue_GetThreadLimit(void);

GRAPI	void GRCC grThreadQueue_Sleep(int Milliseconds);
				// don't use the Windows Sleep() use this

GRAPI	void GRCC grThreadQueue_PollJobs(void);
				// warning : do NOT call this unless you are the master of the threads!
				//	calling this function too often will under-represent high priority threads!
				//	try to use WaitOnJob instead!

GRAPI grBoolean GRCC grThreadQueue_WaitOnJob(grThreadQueue_Job * Job,
											grThreadQueue_JobStatus WaitForStatus);
				//can wait for GR_THREADQUEUE_STATUS_RUNNING or GR_THREADQUEUE_STATUS_COMPLETED
				// waits for Status *or higher* !

// ----- use these Semaphores to lock data that ThreadQueue_Jobs may peek at.

GRAPI grThreadQueue_Semaphore * GRCC grThreadQueue_Semaphore_Create(void);
GRAPI void GRCC grThreadQueue_Semaphore_Lock(		grThreadQueue_Semaphore * S);
GRAPI void GRCC grThreadQueue_Semaphore_UnLock(	grThreadQueue_Semaphore * S);
GRAPI void GRCC grThreadQueue_Semaphore_Destroy(	grThreadQueue_Semaphore ** pS);

#ifndef NDEBUG
GRAPI	void GRCC grThreadQueue_DumpQueue(void);			// uses stdio !
#else
#define grThreadQueue_DumpQueue()
#endif

#ifndef NDEBUG
GRAPI void GRCC grThreadQueue_GetDebugInfo(int * pActiveJobCount,int *pSemaphoreCount, int * pNumThreads);
#else
#define grThreadQueue_GetDebugInfo(a,s,n)
#endif


#ifdef	__cplusplus
}
#endif

#endif

