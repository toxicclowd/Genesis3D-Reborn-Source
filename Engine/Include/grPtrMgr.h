/*!
	@file grPtrMgr.h
	
	@author John Pollard
	@brief Helper for resource load and save

	@par Licence
	The contents of this file are subject to the Genesis3D: Reborn Public License       
	Version 1.02 (the "License"); you may not use this file except in         
	compliance with the License. You may obtain a copy of the License at       
	http://www.genesis3d.com                                                        
                                                                             
	@par
	Software distributed under the License is distributed on an "AS IS"           
	basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See           
	the License for the specific language governing rights and limitations          
	under the License.                                                               
                                                                                  
	@par
	The Original Code is Genesis3D: Reborn, released December 12, 1999.                            
	Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C. All Rights Reserved           
*/

#ifndef GR_PTRMGR_H
#define GR_PTRMGR_H

#include "BaseType.h"
#include "VFile.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! @typedef grPtrMgr
	@brief A instance of a resource pointer helper
*/
typedef struct grPtrMgr grPtrMgr;

typedef struct grResourceMgr grResourceMgr;

//=======================================================================================
//	Function prototypes
//=======================================================================================
/*! @fn grPtrMgr* grPtrMgr_Create(void)
	@brief Create a default grPtrMgr instance.
	@return The instance of grPtrMgr or NULL if failed
*/
GRAPI grPtrMgr*	GRCC grPtrMgr_Create(void);

/*! @fn grBoolean grPtrMgr_IsValid(const grPtrMgr *PtrMgr)
	@brief Test the validity of the grPtrMr.
	
	@param[in] PtrMgr The instance subject of the validity test
	@return GR_TRUE if PtrMgr is valid, GR_FALSE otherwise
*/
GRAPI grBoolean	GRCC grPtrMgr_IsValid(const grPtrMgr *PtrMgr);

/*! @fn grBoolean grPtrMgr_CreateRef(grPtrMgr *PtrMgr)
	@brief Create a reference of the PtrMgr instance parameter.
	
	@param[in] PtrMgr The instance to reference
	@return GR_TRUE if succeed, GR_FALSE otherwise
*/
GRAPI grBoolean	GRCC grPtrMgr_CreateRef(grPtrMgr *PtrMgr);

/*! @fn void grPtrMgr_Destroy(grPtrMgr **PtrMgr);
	@brief Decrement the PtrMgr reference counter and destroy it if counter reaches 0.
	
	@param[in] PtrMgr The PtrMgr to dereference and destroy if needed
*/
GRAPI void			GRCC grPtrMgr_Destroy(grPtrMgr **PtrMgr);

/*! @fn grBoolean grPtrMgr_ReadPtr(grPtrMgr *PtrMgr, grVFile *VFile, void **Ptr)
	@brief Reads the ptr header, and determines if the ptr is in the ptr stack.  If in stack, it refs it by 1
	
	@param[in] PtrMgr The grPtrMgr instance used to parse
	@param[in] VFile The grVFile from where #grPtrMgr read
	@param[in] Ptr The object/item read from the file #VFile
	@return GR_TRUE if succeed, GR_FALSE otherwise
*/
GRAPI grBoolean	GRCC grPtrMgr_ReadPtr(grPtrMgr *PtrMgr, grVFile *VFile, void **Ptr);

/*! @fn grBoolean grPtrMgr_PushPtr(grPtrMgr *PtrMgr, void *Ptr)
	@brief Pushes a pointer onto the ptr stack.
	
	@param[in] PtrMgr The grPtrMgr instance used to parse
	@param[in] Ptr The object/item to keep information we have seen it
	@return GR_TRUE if succeed, GR_FALSE otherwise
*/
GRAPI grBoolean	GRCC grPtrMgr_PushPtr(grPtrMgr *PtrMgr, void *Ptr);

/*! @fn grBoolean grPtrMgr_WritePtr(grPtrMgr *PtrMgr, grVFile *VFile, void *Ptr, uint32 *Count)
	@brief Write the ptr header and returns the current ref count of the ptr in the stack (0 == not in stack yet).
	
	@param[in] PtrMgr The grPtrMgr instance used to parse
	@param[in] VFile The file where to write Ptr header
	@param[in] Ptr The object/item to write indexes
	@param[out] Count The number of time we have seen this pointer
	@return GR_TRUE if succeed, GR_FALSE otherwise
*/
GRAPI grBoolean	GRCC grPtrMgr_WritePtr(grPtrMgr *PtrMgr, grVFile *VFile, void *Ptr, uint32 *Count);

/*! @fn void grPtrMgr_PopPtr(grPtrMgr *PtrMgr, void *Ptr)
	@brief Pops the last pushed ptr off the stack (Must specify the pointer for internal error checking).
	
	@param[in] PtrMgr The grPtrMgr instance used to parse
	@param[in] Ptr The pointer to pop
	@return GR_TRUE if succeed, GR_FALSE otherwise
*/
GRAPI void			GRCC grPtrMgr_PopPtr(grPtrMgr *PtrMgr, void *Ptr);

/*! @fn grBoolean grPtrMgr_GetPtrCount(const grPtrMgr *PtrMgr, int32 *PtrCount)
	@brief Pops the last pushed ptr off the stack (Must specify the pointer for internal error checking).
	
	@param[in] PtrMgr The grPtrMgr instance used to parse
	@param[out] PtrCount The number of pointer in the ptr stack
	@return GR_TRUE if succeed, GR_FALSE otherwise
*/
GRAPI grBoolean	GRCC grPtrMgr_GetPtrCount(const grPtrMgr *PtrMgr, int32 *PtrCount);

/*! @fn grBoolean grPtrMgr_GetPtrRefs(const grPtrMgr *PtrMgr, int32 *PtrRefs)
	@brief Pops the last pushed ptr off the stack (Must specify the pointer for internal error checking).
	
	@param[in] PtrMgr The grPtrMgr instance used to parse
	@param[out] PtrRefs The total number of pointer references (count of each pointer) in the ptr stack
	@return GR_TRUE if succeed, GR_FALSE otherwise
*/
GRAPI grBoolean	GRCC grPtrMgr_GetPtrRefs(const grPtrMgr *PtrMgr, int32 *PtrRefs);

/*! @fn grResourceMgr* grPtrMgr_GetResourceMgr(const grPtrMgr *PtrMgr)
	@brief Current grResourceMgr accessor

	@param[in] PtrMgr The grPtrMgr instance
	@return The current grResourceMgr or NULL if no Resource Manager set
*/
GRAPI grResourceMgr* GRCC grPtrMgr_GetResourceMgr(const grPtrMgr *PtrMgr);

/*! 
@page ptrmgr The Pointer Manager
@section goal Description
@par
#grPtrMgr keep a stack of all resources pointers opened. It is used to avoid duplication of resources sharing the same pointer.<br>
When writing, engine adds to the #grPtrMgr stack all addresses of items it parses. Instead of writing the complete object, it only write the index in the stack.<br>
When loading, engine recreate the stack of the grPtrMgr when recreating all items. Engine queries the #grPtrMgr stack for each item it tries to create. If the object is known in
the stack, the #grPtrMgr instance will return its indexes. If not, the indexes returned is <b>-1</b>.
@section samples Examples
@par Example of read code
@code
grActor *grActor_CreateFromFile(grVFile *VFile, grPtrMgr *PtrMgr)
{
	if (PtrMgr)
	{
		if (!grPtrMgr_ReadPtr(PtrMgr, VFile, &Actor))
			return NULL;

		if (Actor)
		{
			if (!grActor_CreateRef(Actor))
				return NULL:

			return Actor;		// Ptr found in stack, return it
		}
	}

	// Create a new actor
	Actor = GR_RAM_ALLOCATE_STRUCT(grActor);

	if (!Actor)
		return NULL;
	
	if (!grVFile_Read(VFile, &Actor->Number, sizeof(Actor->Number))
		goto ExitWithError;

	if (PtrMgr)
	{
		// Push the ptr on the stack
		if (!grPtrMgr_PushPtr(PtrMgr, Actor))
			goto ExitWithError;
	}
	
	return Actor;

	ExitWithError:
	{
		if (Actor)
			grRam_Free(Actor);

		return NULL;
	}
}
@endcode

@par Example of write code
@code
grBoolean grActor_WriteToFile(const grActor *Actor, grVFile *VFile, grPtrMgr *PtrMgr)
{
	uint32		Count;

	if (PtrMgr)
	{
		if (!grPtrMgr_WritePtr(PtrMgr, VFile, Actor, &Count))
			return GR_FALSE;

		if (Count)		// Already loaded
			return GR_TRUE;
	}

	if (!grVFile_Write(VFile, &Actor->Number, sizeof(Actor->Number))
		return GR_FALSE:
	
	if (PtrMgr)
	{
		// Push the ptr on the stack
		if (!grPtrMgr_PushPtr(PtrMgr, Actor))
			return GR_FALSE;
	}

	return GR_TRUE;
}
@endcode
*/


#ifdef __cplusplus
}
#endif


//========================================================================================
// Backward Compatibility Definitions (je -> gr)
//========================================================================================

#endif


