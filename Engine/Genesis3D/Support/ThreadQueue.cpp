/****************************************************************************************/
/*  THREADQUEUE.C                                                                       */
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
#include	<windows.h>

#include	<assert.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<process.h>

#include	"ThreadQueue.h"
#include	"mempool.h"
#include	"log.h"
#include	"threadlog.h"

#ifdef	__BORLANDC__
#define grRam_Allocate malloc
#define grRam_Free free
#else
#include	"ram.h"
#endif

#define	THREADSTACKSIZE	(0x10000)
#define	MAX_THREADS		(100)	// this is just the static array size

/*}{******** The Types **********/

typedef	enum
{
	TS_NOTSTARTED,
	TS_FREE=0,
	TS_PENDING,
	TS_OUGHTTOBERUNNING,
	TS_RUNNING,
	TS_DONE,
}	Thread_State;

//typedef	void (*Thread_Function)(Thread *, void *);
typedef	void (*Thread_Function)(void *);

typedef	struct	Thread
{
	Thread_State		State;
	HANDLE				ThreadStallingEvent;
	Thread_Function		Function;
	void *				Context;
	grBoolean			Terminate;
//	HANDLE				Handle;
}	Thread;

typedef	struct	ThreadPool
{
	Thread				Threads[MAX_THREADS];
//	int					MaxThreads;
	int					NumThreads;
	CRITICAL_SECTION	CS;
}	ThreadPool;

#define JOB_SIGNATURE 0xFEEDFEED

typedef	struct	grThreadQueue_Job
{
//	HANDLE				ThreadHandle;
	uint32				Signature;
	grThreadQueue_JobStatus	Status;
	int					RefCount;
//	grAsyncStatus		Status;
//	grBoolean			Active;

	grThreadQueue_JobFunction	Function;
	void *				Context;
	grErrorLog *		ErrorLog;
//	uint32				StackLimit;

	grThreadQueue_Job *	Next;
	grThreadQueue_Job *	Prev;
	Thread *			Thread;

}	grThreadQueue_Job;


/*}{******** The Statics that represent the active Pool **********/

/*
		ActiveJobCount is the count of jobs that have threads assigned.
*/
static	int					ActiveJobCount = 0,MaxActiveJobs = MAX_THREADS;

static	ThreadPool *		GlobalThreadPool = NULL;

/*
		JobList is a circular list of jobs.  Jobs at the front are high
		priority.  Jobs at the back are low priority.
*/
#pragma warning (disable:4152)	// nonstandard extension, function/data pointer conversion in expression
static	grThreadQueue_Job 	JobList =
{
//	(HANDLE)-1,
	JOB_SIGNATURE,
	GR_THREADQUEUE_STATUS_COMPLETED,
	1,
	//(void *)0xBEEFFACE,
	//(void *)0xCAFEDEAD,
	NULL,
	NULL,
	0,
};
#pragma warning (default:4152)

/*
		QueueLock is a critical section to guard access to the queue and
		status information.
*/
static	CRITICAL_SECTION	QueueLock;
//static	grBoolean			QueueLockFlag;

/*
		TQInitialized is whether we've initialized the system.
*/
static	grBoolean			TQInitialized = GR_FALSE;

/*}{******** Functions **********/

static	void	LockQueue(void)
{
	EnterCriticalSection(&QueueLock);
//	QueueLockFlag = GR_TRUE;
}

static	void	UnlockQueue(void)
{
//	QueueLockFlag = GR_FALSE;
	LeaveCriticalSection(&QueueLock);
}

static	void __cdecl ThreadFunction(void *Context)
{
	Thread *	T;

	T = (Thread*)Context;

	for	(;;)
	{
		WaitForSingleObject(T->ThreadStallingEvent, INFINITE);

		ActiveJobCount ++;
		T->State = TS_RUNNING;

		(T->Function)(T->Context);

		T->State = TS_DONE;
		ActiveJobCount --;
	}
}

void	Thread_Run(Thread *T, Thread_Function Function, void *Context)
{
	assert(T->State == TS_PENDING);

	T->Function = Function;
	T->Context = Context;
	T->State = TS_OUGHTTOBERUNNING;

	PulseEvent(T->ThreadStallingEvent);
}

grBoolean ThreadPool_Destroy(ThreadPool *Pool)
{
	int	i;

	assert(Pool);

	for	(i = 0; i < Pool->NumThreads; i++)
	{
		if	(Pool->Threads[i].State != TS_FREE)
		{
			grErrorLog_AddString(-1,"ThreadQueue : cannot free threadpool : threads still running!",NULL);
			return GR_FALSE;
		}

		CloseHandle(Pool->Threads[i].ThreadStallingEvent);
	}

	DeleteCriticalSection(&Pool->CS);
	grRam_Free(Pool);

	return GR_TRUE;
}

grBoolean InitThread(Thread * T)
{
	assert(T);

	memset(T,0,sizeof(*T));

	T->State = TS_FREE;
	T->ThreadStallingEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if ( T->ThreadStallingEvent == NULL )
	{
		return GR_FALSE;
	}

	if ( _beginthread(ThreadFunction, THREADSTACKSIZE, T) == -1 )
	{
		CloseHandle(T->ThreadStallingEvent);
		return GR_FALSE;
	}

return GR_TRUE;
}

ThreadPool *	ThreadPool_Create(void)
{
ThreadPool *	Pool;

	Pool = (ThreadPool*)grRam_AllocateClear(sizeof(*Pool));
	if	(!Pool)
		return Pool;

	InitializeCriticalSection(&Pool->CS);

	Pool->NumThreads = 0;

return Pool;
}

Thread * ThreadPool_GetFreeThread(ThreadPool *Pool)
{
	Thread *	Result;
	int			i;

	EnterCriticalSection(&Pool->CS);
	Result = NULL;
	for	(i = 0; i < Pool->NumThreads; i++)
	{
	Thread * T;
		T = Pool->Threads + i;
#if 1 // @@
		if ( T->State == TS_FREE || T->State == TS_DONE )
#else
		if ( T->State == TS_FREE )
#endif
		{
			Result = T;
			Result->State = TS_PENDING;
			break;
		}
		// CB : why do we do this *after* the check?
		#pragma message("ThreadPool : GetFreethread : CB wierd DONE -> FREE !")
#if 0 // @@
		if	(Pool->Threads[i].State == TS_DONE)
			Pool->Threads[i].State = TS_FREE;
#endif
	}
	if ( ! Result )
	{
		// with MaxActiveJobCount, we should never hit this condition,
		//	but with all the sync-up problems, what the hey?
		if ( Pool->NumThreads < MAX_THREADS )
		{
			Result =&(Pool->Threads[Pool->NumThreads]);
			if ( InitThread(Result) )
			{
				Pool->NumThreads++;
				Result->State = TS_PENDING;
			}
			else
			{
				Result = NULL;
			}
			Log_Printf("ThreadQueue : extending pool to %d threads\n",Pool->NumThreads);
		}
	}
	LeaveCriticalSection(&Pool->CS);

	return Result;
}

static	grBoolean	InitTQ(void)
{
	if ( TQInitialized == GR_FALSE )
	{
		JobList.Next = &JobList;
		JobList.Prev = &JobList;
		InitializeCriticalSection(&QueueLock);

		TQInitialized = GR_TRUE;
	}

	if ( ! 	GlobalThreadPool )
	{
		GlobalThreadPool = ThreadPool_Create();
		if ( ! GlobalThreadPool )
			return GR_FALSE;
	}

	return GR_TRUE;
}

typedef	enum
{
	CHECK_FORWARD,
	CHECK_BACKWARD,
}	DebugDirection;

static	void	PutBreakPointHere(void)
{
	OutputDebugString("CheckQueue is about to fail\r\n");
}

static	grBoolean	CheckLockedQueue(DebugDirection Direction)
{
static	grThreadQueue_Job *	Runner;
static	grThreadQueue_Job *	Runner2x;

	/*
		Some invariants:
	*/
	if	(JobList.Status != GR_THREADQUEUE_STATUS_COMPLETED)
	{
		PutBreakPointHere();
		return GR_FALSE;
	}
#if 0
	if	(JobList.ThreadHandle != (HANDLE)-1)
	{
		PutBreakPointHere();
		return GR_FALSE;
	}
#endif
	if	(JobList.Function != (grThreadQueue_JobFunction)0xBEEFFACE)
	{
		PutBreakPointHere();
		return GR_FALSE;
	}
	if	(JobList.Context != (void *)0xCAFEDEAD)
	{
		PutBreakPointHere();
		return GR_FALSE;
	}

//	if	(JobList.StackLimit != 0)
//	{
//		PutBreakPointHere();
//		return GR_FALSE;
//	}

	if	(Direction == CHECK_FORWARD)
	{
		Runner = JobList.Next;
		Runner2x = JobList.Next->Next;
	}
	else
	{
		Runner = JobList.Prev;
		Runner2x = JobList.Prev->Prev;
	}

	//  No jobs in the list?
	if	(JobList.Next == &JobList)
	{
		if	(JobList.Prev != &JobList)
		{
			PutBreakPointHere();
			return GR_FALSE;
		}
		return GR_TRUE;
	}

	do
	{
		if	(Runner->Status < GR_THREADQUEUE_STATUS_WAITINGFORTHREAD ||
			 Runner->Status > GR_THREADQUEUE_STATUS_COMPLETED)
		{
			PutBreakPointHere();
			return GR_FALSE;
		}

		//  Is there a loop?
		if	(Runner == Runner2x)
		{
			PutBreakPointHere();
			return GR_FALSE;
		}

		if	(Direction == CHECK_FORWARD)
		{
			Runner = Runner->Next;
			Runner2x = Runner2x->Next->Next;
		}
		else
		{
			Runner = Runner->Prev;
			Runner2x = Runner2x->Prev->Prev;
		}

	}	while	(Runner != &JobList && Runner2x != &JobList);

	return GR_TRUE;
}

static	grBoolean	CheckQueue(DebugDirection Direction)
{
	grBoolean	Result;

	LockQueue();
	Result = CheckLockedQueue(Direction);
	UnlockQueue();
	return Result;
}

static	void ThreadStart(void *Context)
{
	grThreadQueue_Job *	Job;

	Job = (grThreadQueue_Job*)Context;

	Job->Status = GR_THREADQUEUE_STATUS_RUNNING;
	(Job->Function)(Job, Job->Context);
	assert(Job->Signature == JOB_SIGNATURE);
	Job->Status = GR_THREADQUEUE_STATUS_COMPLETED;
}

GRAPI	void GRCC grThreadQueue_Sleep(int Milliseconds)
{
	Sleep(Milliseconds);
}

GRAPI	grThreadQueue_JobStatus	GRCC grThreadQueue_JobGetStatus(const grThreadQueue_Job *Job)
{
	return Job->Status;
}

static	void	ActivateJob(grThreadQueue_Job *Job)
{
	Thread *			T;

	T = ThreadPool_GetFreeThread(GlobalThreadPool);
	if	(!T)
		return;

	Job->Status = GR_THREADQUEUE_STATUS_WAITINGTOBEGIN;
	Job->Thread = T;
	Thread_Run(T, ThreadStart, Job);
}

GRAPI	void GRCC grThreadQueue_PollJobs(void)
{
	grThreadQueue_Job *	Jobs;

	if	(InitTQ() == GR_FALSE)
	{
#pragma message ("ThreadQueue_PollJobs: Need to be able to propagate err-ors")
		return;
	}

	assert(CheckQueue(CHECK_FORWARD ) == GR_TRUE);
	assert(CheckQueue(CHECK_BACKWARD) == GR_TRUE);

#pragma message ("ThreadQueue_PollJobs: I must not be called from multiple threads!")

	if ( ActiveJobCount >= MaxActiveJobs )
		return;

	LockQueue();
	Jobs = JobList.Next;
	while	(Jobs != &JobList && (ActiveJobCount < MaxActiveJobs))
//	while	(Jobs != &JobList)
	{
		if	(Jobs->Status == GR_THREADQUEUE_STATUS_WAITINGTOBEGIN)
		{
			if	(Jobs->Thread && Jobs->Thread->State == TS_OUGHTTOBERUNNING)
				PulseEvent(Jobs->Thread->ThreadStallingEvent);
		}

		if	(Jobs->Status == GR_THREADQUEUE_STATUS_WAITINGFORTHREAD)
		{
			ActivateJob(Jobs);
			break;
		}
		Jobs = Jobs->Next;
	}
	UnlockQueue();
}

GRAPI grThreadQueue_Job *	GRCC grThreadQueue_JobCreate(
	grThreadQueue_JobFunction		Function,
	void *		Context,
	grErrorLog *ErrorLog,
	uint32		StackLimit)
{
	grThreadQueue_Job *	Job;

#pragma message("ThreadQueue_JobCreate : remove StackLimit parameter")

	InitTQ();

#pragma message("ThreadQueue : use MemPool for Jobs (?)")

	Job = (grThreadQueue_Job*)grRam_AllocateClear(sizeof(*Job));
	if	(!Job)
		return Job;

	Job->Signature	= JOB_SIGNATURE;
	Job->Function	= Function;
	Job->Context	= Context;
	Job->ErrorLog	= ErrorLog;
//	Job->StackLimit	= StackLimit;
	Job->Status = GR_THREADQUEUE_STATUS_WAITINGFORTHREAD;
	Job->RefCount = 1;

	/*
		New jobs always start out as low priority - they go to the
		back of the list.
	*/
	LockQueue();
	Job->Next = &JobList;
	Job->Prev = JobList.Prev;
	JobList.Prev->Next = Job;
	JobList.Prev = Job;
	UnlockQueue();

	return Job;
}

GRAPI	void GRCC grThreadQueue_JobCreateRef(grThreadQueue_Job *Job)
{
	assert(Job);
	LockQueue();
	Job->RefCount++;
	UnlockQueue();
}

GRAPI	void GRCC grThreadQueue_JobDestroy(grThreadQueue_Job **pJob)
{
	grThreadQueue_Job *	Job;

	LockQueue();

	assert(pJob);
	assert(*pJob != &JobList);

	Job = *pJob;

	Job->RefCount--;
	
	if	(Job->RefCount)
	{
		UnlockQueue();
		return;
	}

	Job->Prev->Next = Job->Next;
	Job->Next->Prev = Job->Prev;

	UnlockQueue();

	assert(Job->Signature == JOB_SIGNATURE);
	assert(Job->Status == GR_THREADQUEUE_STATUS_COMPLETED);

	grRam_Free(Job);

	*pJob = NULL;
}

GRAPI	grBoolean GRCC grThreadQueue_JobSetPriority(
	grThreadQueue_Job *		Job,
	grThreadQueue_Priority	Priority)
{
	if	(Job->Status != GR_THREADQUEUE_STATUS_WAITINGFORTHREAD)
		return GR_FALSE;

	LockQueue();

	//  Take it out of the list:
	Job->Next->Prev = Job->Prev;
	Job->Prev->Next = Job->Next;

	if	(Priority == GR_THREADQUEUE_PRIORITY_HIGH)
	{
		JobList.Next->Prev = Job;
		Job->Next = JobList.Next;
					JobList.Next = Job;
		Job->Prev = &JobList;
	}
	else
	{
		assert(Priority == GR_THREADQUEUE_PRIORITY_LOW);
		JobList.Prev->Next = Job;
		Job->Prev = JobList.Prev;
					JobList.Prev = Job;
		Job->Next = &JobList;
	}

	UnlockQueue();

	return GR_TRUE;
}

GRAPI	grThreadQueue_Priority GRCC grThreadQueue_JobGetPriority(
	grThreadQueue_Job *		Job)
{
	int					i;
	grThreadQueue_Job *	Jobs;

	LockQueue();

	Jobs = JobList.Next;
	i = 0;
	while	(Jobs != &JobList)
	{
		if	(Jobs == Job)
		{
			UnlockQueue();
			return GR_THREADQUEUE_PRIORITY_HIGH;
		}
		i++;
		Jobs = Jobs->Next;
#if 1
		if	(i >= MaxActiveJobs)
		{
			UnlockQueue();
			return GR_THREADQUEUE_PRIORITY_LOW;
		}
#endif
	}


	assert(!"Should never get here");
	return GR_THREADQUEUE_PRIORITY_LOW;
}

GRAPI	grBoolean GRCC grThreadQueue_SetThreadLimit(int MaxThreads)
{
	if ( MaxThreads >= MAX_THREADS )
		return GR_FALSE;
	MaxActiveJobs = MaxThreads;
	return GR_TRUE;
}

GRAPI	int GRCC grThreadQueue_GetThreadLimit(void)
{
	return MaxActiveJobs;
//	return MAX_THREADS;
}

#ifndef NDEBUG
GRAPI	void GRCC grThreadQueue_DumpQueue(void)
{
	grThreadQueue_Job *	Jobs;

	printf("ThreadQueue: Dump of threads\n");
	printf("------------------------------\n");

	Jobs = JobList.Next;
	while	(Jobs != &JobList)
	{
		if	(Jobs->Status == GR_THREADQUEUE_STATUS_WAITINGFORTHREAD)
			printf("<%08x> Waiting\n", Jobs);
		if	(Jobs->Status == GR_THREADQUEUE_STATUS_RUNNING)
			printf("<%08x> Running\n", Jobs);
		if	(Jobs->Status == GR_THREADQUEUE_STATUS_COMPLETED)
			printf("<%08x> Completed\n", Jobs);
		Jobs = Jobs->Next;
	}
}
#endif

GRAPI grBoolean GRCC grThreadQueue_WaitOnJob(grThreadQueue_Job * Job,
											grThreadQueue_JobStatus WaitForStatus)
{
grThreadQueue_JobStatus Status;

	assert( Job );

	if ( WaitForStatus != GR_THREADQUEUE_STATUS_RUNNING &&
		 WaitForStatus != GR_THREADQUEUE_STATUS_COMPLETED )
		return GR_FALSE;

	ThreadLog_Printf("WaitOnJob\n");

	Status = grThreadQueue_JobGetStatus(Job);

	if ( Status < WaitForStatus )
	{
	int Waits1=0,Waits2=0;

		grThreadQueue_JobSetPriority(Job,GR_THREADQUEUE_PRIORITY_HIGH);

		while ( Status < GR_THREADQUEUE_STATUS_RUNNING )
		{
			Waits1++;
			assert( Waits1 < 999999 );

			#pragma message("ThreadQueue_WaitOnJob : create emergency threads if all running threads are waiting on non-running threads")

			grThreadQueue_PollJobs();
			grThreadQueue_Sleep(1);
			Status = grThreadQueue_JobGetStatus(Job);
		}
		
		while ( Status < WaitForStatus )
		{
			Waits2++;
			assert( Waits2 < 999999 );
			grThreadQueue_Sleep(1);
			Status = grThreadQueue_JobGetStatus(Job);
		}
	}

return GR_TRUE;
}

/* }{ **** grThreadQueue Semaphore ******/

#define SEMAPHORE_SIGNATURE		((uint32)0xFEEDBABE)

struct grThreadQueue_Semaphore
{
	uint32				Signature1;
	uint32				LockCount;
	CRITICAL_SECTION	CS;
	uint32				Signature2;
};

static MemPool * SemaphorePool = NULL;
static int Semaphores = 0;	// @@ check to see if we have leaks!

GRAPI grThreadQueue_Semaphore * GRCC
	grThreadQueue_Semaphore_Create(void)
{
grThreadQueue_Semaphore * S;

	assert( Semaphores >= 0 );
	if ( ! Semaphores )
	{
		assert( SemaphorePool == NULL );
		SemaphorePool = MemPool_Create(sizeof(grThreadQueue_Semaphore),64,64);
		if ( ! SemaphorePool )
			return NULL;
	}

	S = (grThreadQueue_Semaphore*)MemPool_GetHunk(SemaphorePool);
	if ( ! S )
		return NULL;
	#ifndef NDEBUG
	S->Signature1 = SEMAPHORE_SIGNATURE;
	S->Signature2 = SEMAPHORE_SIGNATURE;
	#endif
	Semaphores++;
	S->LockCount = 0;
	InitializeCriticalSection(&(S->CS));
return S;
}

GRAPI void GRCC grThreadQueue_Semaphore_Lock(grThreadQueue_Semaphore * S)
{
	/* if ( S->LockCount )
	{
		// we're about to stall! try to prevent catastrophes!
		//	this is pointless; if our semaphore is locked, it must be locked by
		//		a running job
		// there's another problem : the same thread can lcok a semaphore many times!
		grThreadQueue_PollJobs();
		grThreadQueue_Sleep(1);
	} */
	assert( S );
	assert( S->Signature1 == SEMAPHORE_SIGNATURE &&
			S->Signature2 == SEMAPHORE_SIGNATURE );
	EnterCriticalSection(&(S->CS));
	
	//assert( ! S->LockCount == 0 || ActiveJobCount > 0 ); // no good, see note above
	S->LockCount ++;
}

GRAPI void GRCC grThreadQueue_Semaphore_UnLock(grThreadQueue_Semaphore * S)
{
	assert(S);
	assert( S->Signature1 == SEMAPHORE_SIGNATURE &&
			S->Signature2 == SEMAPHORE_SIGNATURE );
	assert(S->LockCount > 0);
	S->LockCount --;
	LeaveCriticalSection(&(S->CS));
}

GRAPI void GRCC grThreadQueue_Semaphore_Destroy(grThreadQueue_Semaphore ** pS)
{
	assert( pS );
	if ( *pS )
	{
	grThreadQueue_Semaphore * S = *pS;

		assert( S->Signature1 == SEMAPHORE_SIGNATURE &&
				S->Signature2 == SEMAPHORE_SIGNATURE );
		assert( S->LockCount == 0 );

		DeleteCriticalSection(&(S->CS));
		
		MemPool_FreeHunk(SemaphorePool,S);
		assert( Semaphores > 0 );
		Semaphores--;
		if ( Semaphores == 0 )
		{
			MemPool_Destroy(&SemaphorePool);
			SemaphorePool = NULL;
		}
	}
	*pS = NULL;
}

#ifndef NDEBUG
GRAPI void GRCC grThreadQueue_GetDebugInfo(int * pActiveJobCount,int *pSemaphoreCount, int * pNumThreads)
{
	if ( pActiveJobCount ) *pActiveJobCount = ActiveJobCount;
	if ( pSemaphoreCount ) *pSemaphoreCount = Semaphores;
	if ( pNumThreads ) *pNumThreads = GlobalThreadPool->NumThreads;
}
#endif
