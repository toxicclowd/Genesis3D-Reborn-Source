/****************************************************************************************/
/*  RAM.H                                                                               */
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
// RAM memory manager

#ifndef GR_RAM_H
#define GR_RAM_H

// Memory debugging functionality only supported under Windows..
#ifndef WIN32
#ifndef NDEBUG
	#define REDEFINE_NDEBUGANDDEBUG
	#define NDEBUG
	#undef DEBUG
#endif
#endif

#include "BaseType.h"

#include "grMemAllocInfo.h"	// Added by Icestorm

#ifdef __cplusplus
extern "C" {
#endif

/*******

CB note : do NOT do grRam_Allocate then memset(mem,0,len) !!!
	
use grRam_AllocateClear !

This function uses a very fast memory clearer!  The normal memset
causes L2 cache misses!

*******/

typedef int (* grRam_CriticalCallbackFunction)(void);

/*
  Set the critical callback function.  ram_allocate will call the critical
  callback function if it's unable to allocate memory.
*/
GRAPI grRam_CriticalCallbackFunction GRCC grRam_SetCriticalCallback
    (
      grRam_CriticalCallbackFunction callback
    );

/*
  increments or decrements a counter .  if the counter is >0
  the critical callback function (if set) is called for a failed memory allocation.
  add is added to the current counter value.  the new counter value is returned.
*/
GRAPI int GRCC grRam_EnableCriticalCallback(int add);


/*
  Allocate memory of the given size.  In debug mode, the memory is filled
  with 0xA5, and we keep track of the amount of memory allocated.  Also, in debug
  mode, we track where the memory was allocated and can optionally provide a
  report of allocated blocks.  See grRam_ReportAllocations.
*/

#ifndef NDEBUG

#ifndef GR_DEACTIVATE_JMAI	// Icestorm: Added jMAI+Breakpoint-Support

#define grRam_Allocate(size) (_grRam_DoBreakTest(_grRam_DebugAllocate(size, __FILE__, __LINE__)))

// Do not call _grRam_DebugAllocate directly.
GRAPI void* _grRam_DebugAllocate(uint32 size, const char* pFile, int line);
GRAPI void* GRCC _grRam_DoBreakTest(void *Pointer);	// Icestorm

#else // GR_DEACTIVATE_JMAI

#define grRam_Allocate(size) _grRam_DebugAllocate(size, __FILE__, __LINE__)

// Do not call _grRam_DebugAllocate directly.
GRAPI void* GRCC _grRam_DebugAllocate(uint32 size, const char* pFile, int line);

#endif // GR_DEACTIVATE_JMAI

#else

GRAPI void * GRCC grRam_Allocate(uint32 size);

#endif

// allocate the ram & clear it. (calloc)
#ifndef GR_DEACTIVATE_JMAI //Icestorm : FILE/LINE correction for jMAI

#ifndef NDEBUG

#define grRam_AllocateClear(size) (_grRam_DoBreakTest(_grRam_DebugAllocateClear(size, __FILE__, __LINE__)))

GRAPI void * GRCC _grRam_DebugAllocateClear(uint32 size, const char* pFile, int line);

#else

GRAPI void * GRCC grRam_AllocateClear(uint32 size);

#endif

#ifndef NDEBUG

GRAPI void GRCC grRam_DebugFree_(void *ptr, const char* pFile, int line);

#define grRam_Free(ptr) {grRam_DebugFree_(ptr, __FILE__, __LINE__);(ptr)=NULL;}

#else

GRAPI void GRCC grRam_Free_(void *ptr);

#define grRam_Free(xxx) {grRam_Free_(xxx);(xxx)=NULL;}

#endif

#else // GR_DEACTIVATE_JMAI

// allocate the ram & clear it. (calloc)
GRAPI void * GRCC grRam_AllocateClear(uint32 size);

/*
  Free an allocated memory block.
*/
GRAPI void GRCC grRam_Free_(void *ptr);

#define grRam_Free(xxx) {grRam_Free_(xxx);(xxx)=NULL;}

#endif // GR_DEACTIVATE_JMAI
/*
  Reallocate memory.  This function supports shrinking and expanding blocks,
  and will also act like ram_allocate if the pointer passed to it is NULL.
  It won't, however, free the memory if you pass it a 0 size.
*/
#ifndef NDEBUG

#define grRam_Realloc(ptr, newsize) _grRam_DebugRealloc(ptr, newsize, __FILE__, __LINE__)

// Do not call _grRam_DebugRealloc directly.
GRAPI void* GRCC _grRam_DebugRealloc(void* ptr, uint32 size, const char* pFile, int line);

#else

GRAPI void * GRCC grRam_Realloc(void *ptr,uint32 newsize);

#endif

#ifndef NDEBUG

#include <stdio.h>

GRAPI void GRCC grRam_ReportAllocations(void);

GRAPI void GRCC grRam_ShowStats(FILE * ToFile);

#else

#define grRam_ReportAllocations() 

#endif

#ifndef NDEBUG
    extern int32 grRam_CurrentlyUsed;
    extern int32 grRam_NumberOfAllocations;
    extern int32 grRam_MaximumUsed;
    extern int32 grRam_MaximumNumberOfAllocations;

GRAPI     void GRCC grRam_AddAllocation(int n,uint32 size);
#else
    #define grRam_AddAllocation(n,s)
#endif

#define GR_RAM_ALLOCATE_STRUCT(type)			(type *)grRam_Allocate (sizeof (type))
#define GR_RAM_ALLOCATE_STRUCT_CLEAR(type)      (type *)grRam_AllocateClear(sizeof (type))
#define GR_RAM_ALLOCATE_ARRAY(type,count)		(type *)grRam_Allocate (sizeof (type) * (count))
#define GR_RAM_ALLOCATE_ARRAY_CLEAR(type,count)	(type *)grRam_AllocateClear(sizeof (type) * (count))

#if 0 //{@@

#ifndef NDEBUG	// <> CB note : what the @*#$ is this XX ? This is a bad line, regardless!
#define GR_RAM_REALLOC_ARRAY(ptr,type,count)  (type *)grRam_Realloc(  (ptr), sizeof(type) * (count) );{type *XX=(ptr);}
#else
#define GR_RAM_REALLOC_ARRAY(ptr,type,count)  (type *)grRam_Realloc(  (ptr), sizeof(type) * (count) )
#endif

#else

#define GR_RAM_REALLOC_ARRAY(ptr,type,count)  (type *)grRam_Realloc(  (ptr), sizeof(type) * (count) )

#endif //}

#ifdef NDEBUG
#define grRam_IsValidPtr(ptr)	(GR_TRUE)
#else
grBoolean grRam_IsValidPtr(const void *ptr);
#endif

// Genesis3D: Reborn gr* Aliases


#ifdef __cplusplus
  }
#endif

#ifdef REDEFINE_NDEBUGANDDEBUG
#undef NDEBUG
#define DEBUG
#endif

#endif
