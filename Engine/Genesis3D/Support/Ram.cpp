/****************************************************************************************/
/*  RAM.C                                                                               */
/*                                                                                      */
/*  Author:                                                                             */
/*  Description: Replacement for malloc, realloc and free                               */
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
// RAM Memory manager

//#define DO_REPORT

#ifdef BUILD_BE // Memory debugging functionality only available under Win32
#ifndef NDEBUG

#define BEOS_REDEFINE_NDEBUG
#define NDEBUG
#undef DEBUG

#endif
#endif

#include <memory.h>
#include <malloc.h>
#include <assert.h>

//#ifndef NDEBUG
//#define _CRTDBG_MAP_ALLOC
//#include <crtdbg.h>
//#endif

#ifdef DO_REPORT
#include "Report.h"
REPORT_VARS(MemoryAllocations);
#endif

#include "Ram.h"

#ifndef NDEBUG
#ifndef _LOG
#define _LOG
#endif
#include "Log.h"
#endif

//#define PAD	(8)		// <> use 8 for MMX ? use 16 if on Katmai! // commented out by trilobite dec. 2021
// alternate version by trilobite dec.2021
#define PAD (16)

					// rounds up to the nearest multiple
#define PAD_SIZE(size)	((size+PAD-1)&(~(uint32)(PAD-1)))

/*
  This controls the MINIMAL_CONFIG flag.  Basically, all overflow, underflow,
  and size checking code is always enabled except when NDEBUG is defined...
*/
#ifdef NDEBUG
  // debugging's turned off, so make it minimal config
  #ifndef MINIMAL_CONFIG
	#define MINIMAL_CONFIG
  #endif
#else
  // debugging on, so do full checking
  #ifdef MINIMAL_CONFIG
	#undef MINIMAL_CONFIG
  #endif
#endif

// stupid stuff...
#ifndef JETDLLVERSION
void *StupidUnusedPointer;
#endif

// critical allocation stuff...
static int grRam_CriticalAllocationCount = 0;

static grRam_CriticalCallbackFunction grRam_CriticalCallback = NULL;

/*
  increments or decrements a counter.  if the counter is >0
  the critical callback function (if set) is called for a failed memory allocation.
  add is added to the current counter value.  the new counter value is returned.
*/
GRAPI int GRCC grRam_EnableCriticalCallback(int add)
{
	grRam_CriticalAllocationCount += add;
	return grRam_CriticalAllocationCount;
}


/*
  Set the critical callback function.  grRam_Allocate will call this function
  if it's unable to allocate memory.  Returns the previous critical callback fcn.
*/
GRAPI grRam_CriticalCallbackFunction GRCC grRam_SetCriticalCallback
	(
	  grRam_CriticalCallbackFunction critical_callback
	)
{
	grRam_CriticalCallbackFunction OldCallback;

	OldCallback = grRam_CriticalCallback;
	grRam_CriticalCallback = critical_callback;
	return OldCallback;
}

/*
  If an allocation fails, this function will be called.  If the critical callback
  function is not NULL, then that function will be called.
*/
static int grRam_DoCriticalCallback
	(
	  void
	)
{
	if ((grRam_CriticalAllocationCount != 0) && (grRam_CriticalCallback != NULL))
	{
		return grRam_CriticalCallback ();
	}
	else
	{
		return 0;
	}
}

#ifndef GR_DEACTIVATE_JMAI

#ifdef MINIMAL_CONFIG

	GRAPI void * GRCC grRam_AllocateClear(uint32 size)
	{
	void * mem;
		size = (size + 3)&(~(uint32)3);
		mem = grRam_Allocate(size);
		if ( mem )
		{
			memset (mem, 0, size);
		}
	return mem;
	}
#endif

#else

	GRAPI void * GRCC grRam_AllocateClear(uint32 size)
	{
	void * mem;
		size = (size + 3)&(~(uint32)3);
		mem = grRam_Allocate(size);
		if ( mem )
		{
			memset (mem, 0, size);
		}
	return mem;
	}

#endif

#ifdef MINIMAL_CONFIG

	/*
	  Minimal configuration acts almost exactly like standard malloc, free,
	  and realloc.  The only difference is the critical allocation stuff.
	*/


	/*
	  Allocate memory of the given size.  In debug mode, the memory is filled
	  with 0xA5, and we keep track of the amount of memory allocated.
	*/
	GRAPI void * GRCC grRam_Allocate
		(
		  uint32 size
		)
	{
		void *p;

//		size = PAD_SIZE(size);

		do
		{
			p = malloc(size);
		} while ((p == NULL) && (grRam_DoCriticalCallback ()));


		return p;
	}

	// free an allocated block
	GRAPI void GRCC grRam_Free_
		(
		  void *ptr
		)
	{
	  free (ptr);
	}

	// reallocate a block...
	// This acts like the standard realloc
GRAPI	 void * GRCC grRam_Realloc
		(
		  void *ptr,
		  uint32 newsize
		)
	{
		char *p;
		char * NewPtr;

		if (ptr == NULL)
		{
			return grRam_Allocate (newsize);
		}

		// if newsize is NULL, then it's a free and return NULL
		if (newsize == 0)
		{
			grRam_Free (ptr);
			return NULL;
		}

		p = (char *)ptr;
		do
		{
			NewPtr = (char *)realloc (p, newsize);
		} while ((NewPtr == NULL) && (grRam_DoCriticalCallback ()));

		return NewPtr;
	}

#else  // MINIMAL_CONFIG
	 /*
	   For debugging implementations, we add a header and trailer to the
	   allocated memory blocks so that we compute memory usage, and catch
	   simple over- and under-run errors.
	 */

#ifndef GR_DEACTIVATE_JMAI
	extern void GRCC grMemAllocInfo_Alloc(uint32 Size, void *Pointer, const char *FName, int LNr);
	extern void GRCC grMemAllocInfo_Realloc(uint32 Size, void *Pointer, const char *FName, int LNr);
	extern void GRCC grMemAllocInfo_Free(void *Pointer, const char *FName, int LNr);
	extern grBoolean GRCC grMemAllocInfo_BREAK(const void *Data);
	static int32 grRam_jMAI_Flag = 1;					// Flag for jMAI-Memory-Calls
	static const int DataSize = sizeof (uint32);		//Icestorm:Pointer(to respective jMAI_struct)size
#endif

	// yes, this will break if we use more than 2 gigabytes of RAM...
	int32 grRam_CurrentlyUsed	   = 0;  // total ram currently in use
	int32 grRam_MaximumUsed		 = 0;  // max total ram allocated at any time
	int32 grRam_NumberOfAllocations	 = 0;  // current number of blocks allocated
	int32 grRam_MaximumNumberOfAllocations = 0;  // max number of allocations at any time

	// header and trailer stuff...
	static char MemStamp[] = {"!CHECKME!"};
	static const int MemStampSize = sizeof (MemStamp)-1;
	static const int SizeSize = sizeof (uint32);
	
	// these pads are critical !
#ifndef GR_DEACTIVATE_JMAI
	#define SIZES_SIZE		(SizeSize+DataSize)		// Icestorm: added DataSize...
	#define HEADER_SIZE		PAD_SIZE(SIZES_SIZE		+ MemStampSize)	// Icestorm: added DataSize...
	#define EXTRA_SIZE		PAD_SIZE(HEADER_SIZE	+ MemStampSize)
#else
	#define HEADER_SIZE		PAD_SIZE(SizeSize		+ MemStampSize)
	#define EXTRA_SIZE		PAD_SIZE(HEADER_SIZE	+ MemStampSize)
#endif

	static const unsigned char AllocFillerByte = (unsigned char)0xA5;
	static const unsigned char FreeFillerByte  = (unsigned char)0xB6;

#ifndef GR_DEACTIVATE_JMAI
	/*
	  A memory block is allocated that's size + (2*MemStampSize)+SizeSize+DataSize bytes.
	  It's then filled with 0xA5.  The size stamp is placed at the head of the block,
	  with the MemStamp being placed directly after the pointer at the front, and
	  also at the end of the block.  The layout is:

	  <size><jMAIPointer><MemStamp><<allocated memory>><MemStamp>
	*/
#else
	/*
	  A memory block is allocated that's size + (2*MemStampSize)+SizeSize bytes.
	  It's then filled with 0xA5.  The size stamp is placed at the head of the block,
	  with the MemStamp being placed directly after the size at the front, and
	  also at the end of the block.  The layout is:

	  <size><MemStamp><<allocated memory>><MemStamp>
	*/
#endif

	typedef enum 
	{
		DONT_INITIALIZE = 0, 
		INITIALIZE_MEMORY = 1
	} grRam_MemoryInitialization;

#ifndef GR_DEACTIVATE_JMAI
	// jMAI-Breakpoint-function
	GRAPI void* GRCC _grRam_DoBreakTest(void *Pointer)
	{
		if (grMemAllocInfo_BREAK(*(void**)((uint32)Pointer+SizeSize-HEADER_SIZE)) == GR_TRUE)
			_asm { int 3h }
		return Pointer;
	}
#endif

	static void grRam_SetupBlock
		  (
			char * p,
			uint32 size,
			grRam_MemoryInitialization InitMem
		  )
	{
		if (InitMem == INITIALIZE_MEMORY)
		{
			// fill the memory block
			memset (p+HEADER_SIZE, AllocFillerByte, size);
		}

		// add the size at the front
		*((uint32 *)p) = size;

#ifndef GR_DEACTIVATE_JMAI
		*(void* *)((uint32)p+SizeSize)=NULL;	// Icestorm : DON'T CHANGE THIS!!!
												// Init. jMAI_Ptr to NULL, prevents "death-jumps to nowhere"
		// copy the memstamp to the front of the block
		memcpy (p+SIZES_SIZE, MemStamp, MemStampSize);
#else
		// copy the memstamp to the front of the block
		memcpy (p+SizeSize, MemStamp, MemStampSize);
#endif

		// and to the end of the block
		memcpy (p+HEADER_SIZE+size, MemStamp, MemStampSize);
	}

#ifndef GR_DEACTIVATE_JMAI
#ifndef NDEBUG	// Added File/Line-Support => jMAI can record "real" file/line

GRAPI void * GRCC _grRam_DebugAllocateClear(uint32 size, const char* pFile, int line)
{
void * mem;
	size = (size + 3)&(~(uint32)3);
	mem = _grRam_DebugAllocate(size, pFile, line);
	if ( mem )
	{
		memset (mem, 0, size);
	}
return mem;
}

#else

GRAPI void * GRCC grRam_AllocateClear(uint32 size)
{
void * mem;
	size = (size + 3)&(~(uint32)3);
	mem = grRam_Allocate(size);
	if ( mem )
	{
		memset (mem, 0, size);
	}
return mem;
}

#endif
#endif	// GR_DEACTIVATE_JMAI
/*
This function, (grRam_DebugAllocate) is the source of much misery in the Jet3D engine.
It is the cause of fatal exceptions that prevent applications from initializing, or makes them buggy and unstable.
I wish I could find a way to fix it. -- trilobite dec. 2021.
*/


#ifndef NDEBUG
GRAPI 	void* GRCC _grRam_DebugAllocate(uint32 size, const char* pFile, int line)
	{
	  char *p;

//		size = PAD_SIZE(size);

	  do
	  {
		 
		  p = (char*)malloc(size + EXTRA_SIZE);//, _NORMAL_BLOCK, pFile, line);
	  } while ((p == NULL) && grRam_DoCriticalCallback ());

	  if (p == NULL)
	  {
		 return NULL;
	  }

	  // setup size stamps and memory overwrite checks
	  grRam_SetupBlock (p, size, INITIALIZE_MEMORY);

		grRam_AddAllocation(1,size);

#ifndef GR_DEACTIVATE_JMAI
		if (grRam_jMAI_Flag)
			grMemAllocInfo_Alloc(size, p+SizeSize, pFile, line); //Icestorm
		//NOTE:If this grRam_Alloc is called from jMAI-Module, jMAI_Ptr is still NULL!!
		//     Also deactivated jMAI leave the jMAI_Ptr NULL.
		//     So registered and unregistered memory can be distinguished
#endif

	  return p+HEADER_SIZE;
	}

#else // NDEBUG

GRAPI	 void * GRCC grRam_Allocate (uint32 size)
	{
	  char *p;

//		size = PAD_SIZE(size);

	  do
	  {
		  p = (char*)malloc (size + EXTRA_SIZE);
	  } while ((p == NULL) && grRam_DoCriticalCallback ());

	  if (p == NULL)
	  {
		 return NULL;
	  }

	  // setup size stamps and memory overwrite checks
	  grRam_SetupBlock (p, size, INITIALIZE_MEMORY);

		grRam_AddAllocation(1,size);

	  return p+HEADER_SIZE;
	}
#endif // NDEBUG

	static char * ram_verify_block
		  (
			void * ptr
		  )
	{
		char * p = (char *)ptr;
		uint32 size;

		if (p == NULL)
		{
			assert (0 && "freeing NULL");
			return NULL;
		}

		// make p point to the beginning of the block
		p -= HEADER_SIZE;

		// get size from block
		size = *((uint32 *)p);

#ifndef GR_DEACTIVATE_JMAI
		// check stamp at front
		if (memcmp (p+SIZES_SIZE, MemStamp, MemStampSize) != 0)
		{
			assert (0 && "ram_verify_block:  Memory block corrupted at front");
			return NULL;
		}
#else
		// check stamp at front
		if (memcmp (p+SizeSize, MemStamp, MemStampSize) != 0)
		{
			assert (0 && "ram_verify_block:  Memory block corrupted at front");
			return NULL;
		}
#endif

		// and at back
		if (memcmp (p+HEADER_SIZE+size, MemStamp, MemStampSize) != 0)
		{
			assert (0 && "ram_verify_block:  Memory block corrupted at tail");
			return NULL;
		}

		return p;
	}

#ifndef GR_DEACTIVATE_JMAI

#ifndef NDEBUG
GRAPI	 void GRCC grRam_DebugFree_ (void *ptr, const char* pFile, int line)
	{
		char *p;
		uint32 size;
		void *jMAI_Ptr;	// Icestorm

		// make sure it's a valid block...
		p = ram_verify_block (ptr);
		if (p == NULL)
		{
			return;
		}

		// gotta get the size before you free it
		size = *((uint32 *)p);

		jMAI_Ptr = *(void**)((uint32)p+SizeSize); // Icestorm: Get this jMAI_Ptr

		// fill it with trash...
		memset (p, FreeFillerByte, size+EXTRA_SIZE);

		// free the memory
		free (p);

		// Icestorm Begin
		if(jMAI_Ptr!=NULL)		// (un)registered memoryblock?
		{
			jMAI_Ptr=ram_verify_block(jMAI_Ptr);	//Verify jMAI_Ptr
			if(jMAI_Ptr!=NULL)
				jMAI_Ptr=(void*)((uint32)jMAI_Ptr+HEADER_SIZE);
		} 

		grMemAllocInfo_Free(jMAI_Ptr, pFile, line);
		//Icestorm End

		// update allocations
		grRam_NumberOfAllocations--;
		assert ((grRam_NumberOfAllocations >= 0) && "free()d more ram than you allocated!");

		grRam_CurrentlyUsed -= size;
		assert ((grRam_CurrentlyUsed >= 0) && "free()d more ram than you allocated!");
	}

#else

GRAPI	 void GRCC grRam_Free_ (void *ptr)
	{
		char *p;
		uint32 size;

		// make sure it's a valid block...
		p = ram_verify_block (ptr);
		if (p == NULL)
		{
			return;
		}

		// gotta get the size before you free it
		size = *((uint32 *)p);

		// fill it with trash...
		memset (p, FreeFillerByte, size+EXTRA_SIZE);

		// free the memory
		free (p);

		// update allocations
		grRam_NumberOfAllocations--;
		assert ((grRam_NumberOfAllocations >= 0) && "free()d more ram than you allocated!");

		grRam_CurrentlyUsed -= size;
		assert ((grRam_CurrentlyUsed >= 0) && "free()d more ram than you allocated!");
	}

#endif
#else  //GR_DEACTIVATE_JMAI

GRAPI	 void GRCC grRam_Free_ (void *ptr)
	{
		char *p;
		uint32 size;

		// make sure it's a valid block...
		p = ram_verify_block (ptr);
		if (p == NULL)
		{
			return;
		}

		// gotta get the size before you free it
		size = *((uint32 *)p);

		// fill it with trash...
		memset (p, FreeFillerByte, size+EXTRA_SIZE);

		// free the memory
		free (p);

		// update allocations
		grRam_NumberOfAllocations--;
		assert ((grRam_NumberOfAllocations >= 0) && "free()d more ram than you allocated!");

		grRam_CurrentlyUsed -= size;
		assert ((grRam_CurrentlyUsed >= 0) && "free()d more ram than you allocated!");
	}

#endif  //GR_DEACTIVATE_JMAI

#ifndef NDEBUG

GRAPI	 void * GRCC _grRam_DebugRealloc (void *ptr, uint32 newsize, const char* pFile, int line)
	{
		char *p;
		char * NewPtr;
#ifndef GR_DEACTIVATE_JMAI
		void *jMAI_Ptr;	// Icestorm
#endif
		uint32 size;

		// if realloc is called with NULL, just treat it like an alloc
		if (ptr == NULL)
		{
#ifndef GR_DEACTIVATE_JMAI //Icestorm: added Breakpoint
			return _grRam_DoBreakTest(_grRam_DebugAllocate(newsize, pFile, line));
#else
			return _grRam_DebugAllocate(newsize, pFile, line);
#endif
		}

		// verify the block
		p = ram_verify_block (ptr);
		if (p == NULL)
		{
			return NULL;
		}

		// if newsize is NULL, then it's a free and return NULL
		if (newsize == 0)
		{
#ifndef GR_DEACTIVATE_JMAI 		// Icestorm: again pFile/line-correction
			grRam_DebugFree_(ptr, pFile, line);
#else
			grRam_Free (ptr);
#endif
			return NULL;
		}

		// gotta get the size before I realloc it...
		size = *((uint32 *)p);

#ifndef GR_DEACTIVATE_JMAI
		jMAI_Ptr = *(void**)((uint32)p+SizeSize); // Icestorm: Get this jMAI_Ptr
#endif

		do
		{
			NewPtr = (char *)realloc(p, newsize+EXTRA_SIZE);//, _NORMAL_BLOCK, pFile, line);
		} while ((NewPtr == NULL) && grRam_DoCriticalCallback ());

		// if allocation failed, return NULL...
		if (NewPtr == NULL)
		{
			return NULL;
		}

		grRam_SetupBlock (NewPtr, newsize, DONT_INITIALIZE);

		grRam_AddAllocation(0,newsize - size);

#ifndef GR_DEACTIVATE_JMAI
		if(jMAI_Ptr!=NULL)		// (un)registered?
		{
			jMAI_Ptr=ram_verify_block(jMAI_Ptr);	//Check jMAI_Ptr
			if(jMAI_Ptr!=NULL)
				jMAI_Ptr=(void*)((uint32)jMAI_Ptr+HEADER_SIZE);
		} 

		*(void* *)((uint32)NewPtr+SizeSize)=jMAI_Ptr;	//Restore jMAI_Ptr or set unregistered-Mark(NULL)
		grMemAllocInfo_Realloc(newsize, NewPtr+SizeSize, pFile, line);
#endif

		return NewPtr + HEADER_SIZE;
	}

#else // NDEBUG

GRAPI	 void * grRam_Realloc (void *ptr, uint32 newsize)
	{
		char *p;
		char * NewPtr;
		uint32 size;

		// if realloc is called with NULL, just treat it like an alloc
		if (ptr == NULL)
		{
			return grRam_Allocate (newsize);
		}

		// verify the block
		p = ram_verify_block (ptr);
		if (p == NULL)
		{
			return NULL;
		}

		// if newsize is NULL, then it's a free and return NULL
		if (newsize == 0)
		{
			grRam_Free (ptr);
			return NULL;
		}

		// gotta get the size before I realloc it...
		size = *((uint32 *)p);

		do
		{
			NewPtr = (char *)realloc (p, newsize+EXTRA_SIZE);
		} while ((NewPtr == NULL) && grRam_DoCriticalCallback ());

		// if allocation failed, return NULL...
		if (NewPtr == NULL)
		{
			return NULL;
		}

		grRam_SetupBlock (NewPtr, newsize, DONT_INITIALIZE);

		grRam_AddAllocation(0,newsize - size);

		return NewPtr + HEADER_SIZE;
	}

#endif // NDEBUG

#ifndef NDEBUG

GRAPI void GRCC grRam_ReportAllocations(void)
{
//	_CrtDumpMemoryLeaks();
}

#include <stdio.h>

GRAPI void GRCC grRam_ShowStats(FILE * ToFile)
{
	if ( ! ToFile ) ToFile = stdin;

#ifdef DO_REPORT
	reportFP = ToFile;
	REPORT_REPORT(MemoryAllocations);
#endif

	Log_TeeFile(ToFile);
	Log_Printf("grRam : Used : Currently = %d, Max = %d\n",
		grRam_CurrentlyUsed,grRam_MaximumUsed);
	Log_Printf("grRam : NumAllocs : Currently = %d, Max = %d\n",
		grRam_NumberOfAllocations,grRam_MaximumNumberOfAllocations);
}

#endif

	// for external programs that allocate memory some other way.
	// Here they can use ram to keep track of the memory.
GRAPI	 void GRCC grRam_AddAllocation (int n, uint32 size)
{
	// and update the allocations stuff
	grRam_NumberOfAllocations += n;
	grRam_CurrentlyUsed += size;

	if (grRam_NumberOfAllocations > grRam_MaximumNumberOfAllocations)
	{
		grRam_MaximumNumberOfAllocations = grRam_NumberOfAllocations;
	}
	if (grRam_CurrentlyUsed > grRam_MaximumUsed)
	{
		grRam_MaximumUsed = grRam_CurrentlyUsed;
	}
	
	assert ((grRam_CurrentlyUsed >= 0) && "free()d more ram than you allocated!");

#ifdef DO_REPORT
	{
	int MemoryAllocations;
		MemoryAllocations = size;
		REPORT_ADD(MemoryAllocations);
	}
#endif
}

#endif // MINIMAL_CONFIG


#ifndef GR_DEACTIVATE_JMAI
void grRam_jMAI_Lock()
{
	grRam_jMAI_Flag=0;
}

void grRam_jMAI_UnLock()
{
	grRam_jMAI_Flag=1;
}
#endif //GR_DEACTIVATE_JMAI


#ifndef NDEBUG
grBoolean grRam_IsValidPtr(const void *ptr)
{
const char * p = (const char *)ptr;
uint32 size;

	if (p == NULL) return GR_FALSE;

	// make p point to the beginning of the block
	p -= HEADER_SIZE;

	// get size from block
	size = *((uint32 *)p);

#ifndef GR_DEACTIVATE_JMAI
	// check stamp at front
	if (memcmp (p+SIZES_SIZE, MemStamp, MemStampSize) != 0)
	{
		return GR_FALSE;
	}
#else
	// check stamp at front
	if (memcmp (p+SizeSize, MemStamp, MemStampSize) != 0)
	{
		return GR_FALSE;
	}
#endif // GR_DEACTIVATE_JMAI

	// and at back
	if (memcmp (p+HEADER_SIZE+size, MemStamp, MemStampSize) != 0)
	{
		return GR_FALSE;
	}

return GR_TRUE;
}
#endif

#ifdef BEOS_REDEFINE_NDEBUG
#undef NDEBUG
#define DEBUG
#endif

